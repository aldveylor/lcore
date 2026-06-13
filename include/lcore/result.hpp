#pragma once
#include "base.hpp"
#include "exception.hpp"
#include "traits.hpp"

LCORE_NAMESPACE_BEGIN

template <typename T>
class Error {
public:
    using ValueType = T;
    
private:
    T m_value;

public:
    constexpr Error() = default;
    constexpr Error(T&& value): m_value(std::move(value)) {}
    constexpr Error(const T& value): m_value(value) {}

    template <typename U>
    requires ConvertibleTo<U, T>
    constexpr Error(const Error<U>& other) : m_value(other.Value()) {}

    template <typename U>
    requires ConvertibleTo<U, T>
    constexpr Error(Error<U>&& other) : m_value(std::move(other.Value())) {}

    constexpr const T& Value() const & { return m_value; }
    constexpr T& Value() & { return m_value; }
    constexpr T Value() && { return std::move(m_value); }

    constexpr explicit operator bool() const { return true; }
    constexpr const T* operator->() const { return &m_value; }
    constexpr T* operator->() { return &m_value; }
    constexpr const T& operator*() const { return m_value; }
    constexpr T& operator*() { return m_value; }
};

template <typename T, typename E>
class Result {
public:
    using ValueType = T;
    using ErrorType = LCORE_NAMESPACE::Error<E>;
    using ErrorValueType = E;
    
private:
    union {
        ValueType value;
        ErrorType error;
    };
    bool isOk;

public:
    constexpr Result(): value(), isOk(true) {}
    constexpr Result(ValueType&& v): value(std::move(v)), isOk(true) {}
    constexpr Result(const ValueType& v): value(v), isOk(true) {}
    template <typename... Args>
    requires ConstructibleWith<ValueType, Args...>
    constexpr Result(Args&&... args): isOk(true) {
        ::new (&value) ValueType(std::forward<Args>(args)...);
    }
    constexpr Result(ErrorType&& e): error(std::move(e)), isOk(false) {}
    constexpr Result(const ErrorType& e): error(e), isOk(false) {}
    template <typename U>
    requires ConvertibleTo<U, ErrorValueType>
    constexpr Result(const Error<U>& e): error(e.Value()), isOk(false) {}
    template <typename U>
    requires ConvertibleTo<U, ErrorValueType>
    constexpr Result(Error<U>&& e): error(std::move(e.Value())), isOk(false) {}

    constexpr ~Result() {
        if (isOk) value.~ValueType();
        else error.~ErrorType();
    }
    constexpr Result(const Result& other) : isOk(other.isOk) {
        if (isOk) new (&value) ValueType(other.value);
        else new (&error) ErrorType(other.error);
    }
    constexpr Result(Result&& other) noexcept : isOk(other.isOk) {
        if (isOk) new (&value) ValueType(std::move(other.value));
        else new (&error) ErrorType(std::move(other.error));
        other.isOk = true; // Reset the moved-from state
    }

    constexpr Result& operator=(const Result& other) {
        if (this != &other) {
            this->~Result();
            isOk = other.isOk;
            if (isOk) new (&value) ValueType(other.value);
            else new (&error) ErrorType(other.error);
        }
        return *this;
    }

    constexpr Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            this->~Result();
            isOk = other.isOk;
            if (isOk) new (&value) ValueType(std::move(other.value));
            else new (&error) ErrorType(std::move(other.error));
            other.isOk = true; // Reset the moved-from state
        }
        return *this;
    }

    constexpr bool IsOk() const { return isOk; }
    constexpr bool IsError() const { return !isOk; }
    constexpr ValueType& Value() & {
        if (!isOk) throw RuntimeError("Attempted to access value of an error Result");
        return value;
    }
    constexpr const ValueType& Value() const & {
        if (!isOk) throw RuntimeError("Attempted to access value of an error Result");
        return value;
    }
    constexpr ValueType Value() && {
        if (!isOk) throw RuntimeError("Attempted to access value of an error Result");
        return std::move(value);
    }

    constexpr ErrorValueType& Error() & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error.Value();
    }
    constexpr const ErrorValueType& Error() const & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error.Value();
    }
    constexpr ErrorValueType Error() && {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return std::move(error).Value();
    }

    constexpr ErrorType& AsError() & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error;
    }
    constexpr const ErrorType& AsError() const & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error;
    }
    constexpr ErrorType AsError() && {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return std::move(error);
    }

    constexpr explicit operator bool() const { return isOk; }
    constexpr ValueType* operator->() {
        if (!isOk) throw RuntimeError("Attempted to access value of an error Result");
        return &value;
    }
    constexpr const ValueType* operator->() const {
        if (!isOk) throw RuntimeError("Attempted to access value of an error Result");
        return &value;
    }
    constexpr ValueType& operator*() {
        if (!isOk) throw RuntimeError("Attempted to dereference an error Result");
        return *value;
    }
    constexpr const ValueType& operator*() const {
        if (!isOk) throw RuntimeError("Attempted to dereference an error Result");
        return *value;
    }

    ValueType ValueOr(const ValueType& defaultValue) const {
        return isOk ? value : defaultValue;
    }

    ErrorValueType ErrorOr(const ErrorValueType& defaultError) const {
        return isOk ? defaultError : *error;
    }

    template <typename Func>
    requires InvokeAble<Func, ValueType>
    auto Transform(Func&& func) const {
        if (!isOk) throw RuntimeError("Attempted to transform an error Result");
        using ReturnType = std::invoke_result_t<Func, ValueType>;
        return Result<ReturnType, ErrorValueType>(func(value));
    }

    template <typename Func>
    requires InvokeAble<Func, ErrorValueType>
    auto TransformError(Func&& func) const {
        if (isOk) throw RuntimeError("Attempted to transform a successful Result");
        using ReturnType = std::invoke_result_t<Func, ErrorValueType>;
        return Result<ValueType, ReturnType>(func(error.Value()));
    }

    template <typename Func>
    requires InvokeAble<Func, ValueType>
    auto AndThen(Func&& func) const {
        using ReturnType = std::invoke_result_t<Func, ValueType>;
        using ResultType = Result<ReturnType, ErrorValueType>;
        if (isOk) return ResultType(func(value));
        else return ResultType(error);
    }

    template <typename Func>
    requires InvokeAble<Func, ErrorValueType>
    auto OrElse(Func&& func) const {
        using ReturnType = std::invoke_result_t<Func, ErrorValueType>;
        using ResultType = Result<ValueType, ReturnType>;
        if (isOk) return ResultType(value);
        else return ResultType(func(error.Value()));
    }

    inline T ValueOrThrow() const && {
        if (isOk) return std::move(value);
        else throw std::move(error).Value();
    }
};

template <typename E>
class Result<void, E> {
    using ValueType = void;
    using ErrorType = LCORE_NAMESPACE::Error<E>;
    using ErrorValueType = E;
    
private:
    union {
        char dummy; // Placeholder for void
        ErrorType error;
    };
    bool isOk;
    
public:
    constexpr Result(): dummy(0), isOk(true) {}
    constexpr Result(ErrorType&& e): error(std::move(e)), isOk(false) {}
    constexpr Result(const ErrorType& e): error(e), isOk(false) {}

    constexpr ~Result() {
        if (!isOk) error.~ErrorType();
    }
    constexpr Result(const Result& other) : isOk(other.isOk) {
        if (!isOk) new (&error) ErrorType(other.error);
    }
    constexpr Result(Result&& other) noexcept : isOk(other.isOk) {
        if (!isOk) new (&error) ErrorType(std::move(other.error));
        other.isOk = true; // Reset the moved-from state
    }

    constexpr Result& operator=(const Result& other) {
        if (this != &other) {
            this->~Result();
            isOk = other.isOk;
            if (!isOk) new (&error) ErrorType(other.error);
        }
        return *this;
    }

    constexpr Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            this->~Result();
            isOk = other.isOk;
            if (!isOk) new (&error) ErrorType(std::move(other.error));
            other.isOk = true; // Reset the moved-from state
        }
        return *this;
    }

    constexpr bool IsOk() const { return isOk; }
    constexpr bool IsError() const { return !isOk; }
    constexpr void Value() const {
        if (!isOk) throw RuntimeError("Attempted to access value of an error Result");
    }
    constexpr E& Error() & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error.Value();
    }
    constexpr const E& Error() const & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error.Value();
    }
    constexpr E Error() && {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return std::move(error).Value();
    }

    constexpr ErrorType& AsError() & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error;
    }
    constexpr const ErrorType& AsError() const & {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return error;
    }
    constexpr ErrorType AsError() && {
        if (isOk) throw RuntimeError("Attempted to access error of a successful Result");
        return std::move(error);
    }

    constexpr explicit operator bool() const { return isOk; }

    ErrorValueType ErrorOr(const ErrorValueType& defaultError) const {
        return isOk ? defaultError : *error;
    }

    template <typename Func>
    requires InvokeAble<Func>
    auto Transform(Func&& func) const {
        if (!isOk) throw RuntimeError("Attempted to transform an error Result");
        using ReturnType = std::invoke_result_t<Func>;
        return Result<ReturnType, ErrorValueType>(func());
    }

    template <typename Func>
    requires InvokeAble<Func, ErrorValueType>
    auto TransformError(Func&& func) const {
        if (isOk) throw RuntimeError("Attempted to transform a successful Result");
        using ReturnType = std::invoke_result_t<Func, ErrorValueType>;
        return Result<ValueType, ReturnType>(func(error.Value()));
    }

    template <typename Func>
    requires InvokeAble<Func>
    auto AndThen(Func&& func) const {
        using ReturnType = std::invoke_result_t<Func>;
        using ResultType = Result<ReturnType, ErrorValueType>;
        if (isOk) return ResultType(func());
        else return ResultType(error);
    }

    template <typename Func>
    requires InvokeAble<Func, ErrorValueType>
    auto OrElse(Func&& func) const {
        using ReturnType = std::invoke_result_t<Func, ErrorValueType>;
        using ResultType = Result<ValueType, ReturnType>;
        if (isOk) return ResultType();
        else return ResultType(func(error.Value()));
    }
};

LCORE_NAMESPACE_END

#if defined(__GNUC__) || defined(__clang__)
#define L_TRY(expr) ({ auto _result = (expr); if (!_result.IsOk()) return std::move(_result).AsError(); _result.Value(); })
#define L_TRY_VOID(expr) ({ auto _result = (expr); if (!_result.IsOk()) return std::move(_result).AsError(); })
#else
#define L_TRY(expr) do { static_assert(false, "L_TRY is only supported on GCC and Clang"); } while(0)
#define L_TRY_VOID(expr) do { static_assert(false, "L_TRY_VOID is only supported on GCC and Clang"); } while(0)
#endif

#define L_TRY_SET(var, expr) do { auto _result = (expr); if (!_result.IsOk()) return std::move(_result).AsError(); var = std::move(_result).Value(); } while(0)
#define L_TRY_DEF(var, expr) auto var##_ = (expr); if (!var##_.IsOk()) return std::move(var##_).AsError(); auto var = std::move(var##_).Value();
#define L_TRY_DO(expr) do { auto _result = (expr); if (!_result.IsOk()) return std::move(_result).AsError(); } while(0)
