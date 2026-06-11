#pragma once
#include "base.hpp"
#include "lcore/class.hpp"
#include "traits.hpp"
#include <coroutine>
#include <utility>
#include <optional>

LCORE_ASYNC_NAMESPACE_BEGIN

template <typename InitSuspend = std::suspend_never, typename FinalSuspend = std::suspend_always>
class SuspendHandler {
public:
    using InitSuspendType = InitSuspend;
    using FinalSuspendType = FinalSuspend;

    auto initial_suspend() noexcept { return InitSuspend();}
    auto final_suspend() noexcept { return FinalSuspend(); }
};

template <typename T, typename SuspendHandleType = SuspendHandler<>>
class Promise;

template <typename T, typename SuspendHandlerType = SuspendHandler<>, typename PromiseType = Promise<T, SuspendHandlerType>>
class Task;

template <typename T>
using DefaultTaskWrapper = Task<T, SuspendHandler<>>;

template <typename T, typename SuspendHandleType>
class Promise: public SuspendHandleType {
public:
    using value_type = T;
    using promise_type = Promise<T, SuspendHandleType>;
    using handle_type = std::coroutine_handle<promise_type>;

    Promise() = default;
    ~Promise() = default;

    template <template <typename> typename TaskType = DefaultTaskWrapper>
    TaskType<T> get_return_object(){
        return TaskType<T>(handle_type::from_promise(*this));
    }

    struct final_awaiter {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
            auto& p = h.promise();
            if (p.continuation) p.continuation.resume();
        }
        void await_resume() noexcept {}
    };
    auto final_suspend() noexcept { return final_awaiter{}; }

    void unhandled_exception() noexcept {
        this->exception = std::current_exception();
    }

    void return_value(T&& value) {
        this->value = std::move(value);
    }

    void return_value(const T& value) {
        this->value = value;
    }

    void set_value(T&& value) requires MoveConstructible<T> {
        this->value = std::move(value);
        this->exception = nullptr;
    }
    
    void set_value(const T& value) requires CopyConstructible<T> {
        this->value = value;
        this->exception = nullptr;
    }

    void set_exception(std::exception_ptr exception) noexcept {
        this->exception = exception;
        this->value.reset();
    }

    const T& ref_value_or_exception() const & {
        if(this->exception) std::rethrow_exception(this->exception);
        return this->value.value();
    }

    T peek_value_or_exception() const requires CopyConstructible<T> {
        if(this->exception) std::rethrow_exception(this->exception);
        return this->value.value();
    }

    T consume_value_or_exception() && requires MoveConstructible<T> {
        if(this->exception) std::rethrow_exception(this->exception);
        return std::move(this->value.value());
    }

    bool has_value() const {
        return this->value.has_value();
    }

    bool has_exception() const {
        return this->exception != nullptr;
    }

    const std::optional<T>& get_value_opt() const {
        return this->value;
    }
    const std::exception_ptr& get_exception() const {
        return this->exception;
    }

    std::coroutine_handle<> continuation{};
private:
    std::optional<T> value;
    std::exception_ptr exception;
};

template <typename SuspendHandleType>
class Promise<void, SuspendHandleType>: public SuspendHandleType {
public:
    using value_type = void;
    using promise_type = Promise<void, SuspendHandleType>;
    using handle_type = std::coroutine_handle<promise_type>;

    Promise() = default;
    ~Promise() = default;

    template <template <typename> typename TaskType = DefaultTaskWrapper>
    TaskType<void> get_return_object(){
        return TaskType<void>(handle_type::from_promise(*this));
    }

    struct final_awaiter {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
            auto& p = h.promise();
            if (p.continuation) p.continuation.resume();
        }
        void await_resume() noexcept {}
    };
    auto final_suspend() noexcept { return final_awaiter{}; }

    void unhandled_exception() noexcept {
        this->exception = std::current_exception();
    }

    void return_void(){
    }

    void set_exception(std::exception_ptr exception) noexcept {
        this->exception = exception;
    }

    void get_value_or_exception() {
        if(this->exception) std::rethrow_exception(this->exception);
    }

    bool has_exception() const {
        return this->exception != nullptr;
    }

    const std::exception_ptr& get_exception() const {
        return this->exception;
    }

    std::coroutine_handle<> continuation{};
private:
    std::exception_ptr exception;
};

/// @brief Awaitable base interface, used in co_await
/// @tparam T 
template <typename T>
class AwaitableBase: public Interface {
    bool await_ready() {return false;};
    bool await_suspend(std::coroutine_handle<>) {return false;};
    T await_resume() {return T{};};
};

template <typename T, typename SuspendHandlerType, typename PromiseType>
class Task: AwaitableBase<T>{
public:
    using promise_type = PromiseType;
    struct sentinel{};

    // static_assert(IsPromise<PromiseType>, "PromiseType must be a valid promise type");
private:
    std::coroutine_handle<promise_type> handle;
public:
    Task(): handle(nullptr) {
    }
    Task(std::coroutine_handle<promise_type> handle): handle(handle) {
    }

    Task(const Task&) = delete;
    Task(Task&& other): handle(other.handle) {
        other.handle = nullptr;
    }

    Task& operator=(const Task&) = delete;
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if(handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    std::coroutine_handle<PromiseType> get_handle() const {
        return handle;
    }
    operator std::coroutine_handle<>() const {
        return handle;
    }

    auto operator co_await() && noexcept {
        struct awaiter {
            std::coroutine_handle<promise_type> handle;
            bool await_ready() const noexcept {
                return !handle || handle.done();
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
                handle.promise().continuation = awaiting;
                return handle;
            }
            T await_resume() {
                return std::move(handle.promise()).consume_value_or_exception();
            }
        };
        return awaiter{handle};
    }

    ~Task() {
        if(handle) handle.destroy();
    }

    const T& ref_value() const & {
        return handle.promise().ref_value_or_exception();
    }
    T consume_value() && requires MoveConstructible<T> {
        return handle.promise().consume_value_or_exception();
    }
    bool done() const noexcept { return !handle || handle.done(); }
    bool is_exception() { return handle.promise().has_exception(); }
    std::exception_ptr get_exception() { return handle.promise().get_exception(); }
    void resume() { if(handle) handle.resume(); }
};

template <typename SuspendHandlerType, typename PromiseType>
class Task<void, SuspendHandlerType, PromiseType>: AwaitableBase<void>{
public:
    using promise_type = PromiseType;
    struct sentinel{};

    // static_assert(IsPromise<PromiseType>, "PromiseType must be a valid promise type");
private:
    std::coroutine_handle<promise_type> handle;
public:
    Task(): handle(nullptr){
    }
    Task(std::coroutine_handle<promise_type> handle): handle(handle) {
    }

    Task(const Task&) = delete;
    Task(Task&& other): handle(other.handle) {
        other.handle = nullptr;
    }

    Task& operator=(const Task&) = delete;
    Task& operator=(Task&& other) {
        if (this != &other) {
            if(handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    std::coroutine_handle<PromiseType> get_handle() const {
        return handle;
    }
    operator std::coroutine_handle<>() const {
        return handle;
    }

    auto operator co_await() && noexcept {
        struct awaiter {
            std::coroutine_handle<promise_type> handle;
            bool await_ready() const noexcept {
                return !handle || handle.done();
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
                handle.promise().continuation = awaiting;
                return handle;
            }
            void await_resume() {
                handle.promise().get_value_or_exception();
            }
        };
        return awaiter{handle};
    }

    ~Task() {
        if(handle) handle.destroy();
    }

    void consume_value() {
        handle.promise().get_value_or_exception();
    }
    bool done() const noexcept { return !handle || handle.done(); }
    bool is_exception() { return handle.promise().has_exception(); }
    std::exception_ptr get_exception() { return handle.promise().get_exception(); }
    void resume() { if(handle) handle.resume(); }
};

// Check whether a type is a Task
template <typename T>
struct _is_task : std::false_type {};

template <typename T, typename SuspendHandlerType, typename PromiseType>
struct _is_task<Task<T, SuspendHandlerType, PromiseType>> : std::true_type {};

template <typename T>
concept IsTask = _is_task<std::remove_cvref_t<T>>::value;

LCORE_ASYNC_NAMESPACE_END

