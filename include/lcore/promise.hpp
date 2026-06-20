#pragma once
#include "exception.hpp"
#include "memory.hpp"
#include <exception>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <optional>

LCORE_NAMESPACE_BEGIN

namespace _detail {

template <typename T>
struct PromiseState {
    using Args = std::conditional_t<
        std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(void*),
        T,
        const T&
    >;
    using CallbackType = std::function<void(Args)>;
    using ErrorCallbackType = std::function<void(std::exception_ptr)>;

    std::optional<T> value;
    std::exception_ptr exception;
    std::vector<CallbackType> callbacks;
    std::vector<ErrorCallbackType> error_callbacks;
    std::mutex mutex;
    std::condition_variable cond;
    
    void Complete(Args value) {
        std::vector<CallbackType> _callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (this->value.has_value()) throw RuntimeError("Promise already fulfilled");
            this->value = value;
            _callbacks.swap(callbacks);
        }
        for (const auto& callback : _callbacks) {
            callback(value);
        }
        cond.notify_all();
    }

    void Throw(std::exception_ptr exception) {
        std::vector<ErrorCallbackType> _error_callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (this->value.has_value()) throw RuntimeError("Promise already fulfilled");
            this->exception = exception;
            _error_callbacks.swap(error_callbacks);
        }
        for (const auto& error_callback : _error_callbacks) {
            error_callback(exception);
        }
        cond.notify_all();
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        if (value.has_value()) return;
        cond.wait(lock, [this] { return value.has_value(); });
    }

    bool Done() {
        std::lock_guard<std::mutex> lock(mutex);
        return value.has_value();
    }

    Args Get() {
        std::unique_lock<std::mutex> lock(mutex);
        if (value.has_value()) return *value;
        cond.wait(lock, [this] { return value.has_value(); });
        return *value;
    }

    void Then(CallbackType callback) {
        std::lock_guard<std::mutex> lock(mutex);
        if (value.has_value()) {
            callback(*value);
        } else {
            callbacks.push_back(callback);
        }
    }
};

template <>
struct PromiseState<void> {
    using Args = void;
    using CallbackType = std::function<void()>;
    using ErrorCallbackType = std::function<void(std::exception_ptr)>;

    bool done;
    std::exception_ptr exception;
    std::vector<CallbackType> callbacks;
    std::vector<ErrorCallbackType> error_callbacks;
    std::mutex mutex;
    std::condition_variable cond;

    PromiseState() : done(false) {}
    void Complete() {
        std::vector<CallbackType> _callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (done) throw RuntimeError("Promise already fulfilled");
            done = true;
            _callbacks.swap(callbacks);
        }
        for (const auto& callback : _callbacks) {
            callback();
        }
        cond.notify_all();
    }

    void Throw(std::exception_ptr exception) {
        std::vector<ErrorCallbackType> _error_callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (done) throw RuntimeError("Promise already fulfilled");
            this->exception = exception;
            _error_callbacks.swap(error_callbacks);
        }
        for (const auto& error_callback : _error_callbacks) {
            error_callback(exception);
        }
        cond.notify_all();
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        if (done) return;
        cond.wait(lock, [this] { return done; });
    }

    bool Done() {
        std::lock_guard<std::mutex> lock(mutex);
        return done;
    }

    void Then(CallbackType callback) {
        std::lock_guard<std::mutex> lock(mutex);
        if (done) {
            callback();
        } else {
            callbacks.push_back(callback);
        }
    }
};
}

template <typename T>
class Future;

template <typename T>
class Promise {
public:
    using StateType = _detail::PromiseState<T>;
    using Args = typename StateType::Args;
    using CallbackType = typename StateType::CallbackType;
private:
    SharedPtr<StateType> state;
public:
    Promise() : state(MakeShared<StateType>()) {}

    void Complete(ReplaceIf<Args, void, Monostate> value) requires (!Void<T>) {
        state->Complete(value);
    }

    void Throw(std::exception_ptr exception) {
        state->Throw(exception);
    }

    void Complete() requires Void<T> {
        state->Complete();
    }

    Future<T> GetFuture() const {
        return Future<T>(state);
    }
};

template <typename T>
class Future {
    friend class Promise<T>;
public:
    using StateType = _detail::PromiseState<T>;
    using Args = typename StateType::Args;
    using CallbackType = typename StateType::CallbackType;
    using ErrorCallbackType = typename StateType::ErrorCallbackType;
private:
    SharedPtr<StateType> state;
protected:
    Future(SharedPtr<StateType> state) : state(std::move(state)) {}
public:
    void Wait() const {
        state->Wait();
    }
    auto Get() const requires (!Void<T>) {
        return state->Get();
    }
    bool Done() const {
        return state->Done();
    }
    void Then(CallbackType callback) const {
        state->Then(std::move(callback));
    }
    void Error(ErrorCallbackType error_callback) const {
        state->Then(std::move(error_callback));
    }
};

LCORE_NAMESPACE_END
