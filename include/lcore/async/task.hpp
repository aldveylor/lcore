#pragma once
#include "promise.hpp"

LCORE_NAMESPACE_BEGIN
namespace async {

template <IsPromise PromiseType>
class TaskBase {
public:
    using promise_type = PromiseType;
    using T = typename promise_type::value_type;
    struct sentinel{};

protected:
    std::coroutine_handle<promise_type> handle;
public:
    TaskBase(): handle(nullptr) {
    }
    TaskBase(std::coroutine_handle<promise_type> handle): handle(handle) {
    }

    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&& other): handle(other.handle) {
        other.handle = nullptr;
    }
    TaskBase& operator=(const TaskBase&) = delete;
    TaskBase& operator=(TaskBase&& other) noexcept {
        if (this != &other) {
            if(handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    std::coroutine_handle<promise_type> get_handle() const {
        return handle;
    }
    operator std::coroutine_handle<>() const {
        return handle;
    }

    ~TaskBase() {
        if(handle) handle.destroy();
    }

    bool done() const noexcept { return !handle || handle.done(); }
    void resume() { if(handle) handle.resume(); }
    bool has_exception() { return handle.promise().has_exception(); }
    std::exception_ptr get_exception() { return handle.promise().get_exception(); }

    auto& ref_value() const & requires (!Void<T>) {
        return handle.promise().ref_value_or_exception();
    }
    T consume_value() && requires (!Void<T> && MoveConstructible<T>) {
        return std::move(handle.promise()).consume_value_or_exception();
    }
    T peek_value() const requires (!Void<T> && CopyConstructible<T>) {
        return handle.promise().peek_value_or_exception();
    }
    void rethrow_exception() { handle.promise().rethrow_exception(); }
};

template <typename T>
class EagerTask : public TaskBase<Promise<T, std::suspend_never, EagerTask>> {
public:
    using Base = TaskBase<Promise<T, std::suspend_never, EagerTask>>;
    using Base::Base;
    using promise_type = typename Base::promise_type;

    auto operator co_await() && {
        struct Awaiter {
            std::coroutine_handle<promise_type> handle;
            bool await_ready() const noexcept { return !handle || handle.done(); }
            void await_suspend(std::coroutine_handle<> awaiting) const noexcept {
                if (handle) handle.promise().continuation = awaiting;
            }
            auto await_resume() const requires (!Void<T>) {
                return std::move(handle.promise()).consume_value_or_exception();
            }
            void await_resume() const requires Void<T> {
                handle.promise().rethrow_exception();
            }
            ~Awaiter() {
                if (handle) handle.destroy();
            }
        };
        auto h = this->handle;
        this->handle = nullptr;
        return Awaiter{h};
    }
};

template <typename T>
class LazyTask : public TaskBase<Promise<T, std::suspend_always, LazyTask>> {
public:
    using Base = TaskBase<Promise<T, std::suspend_always, LazyTask>>;
    using Base::Base;
    using promise_type = typename Base::promise_type;
    auto operator co_await() && {
        struct Awaiter {
            std::coroutine_handle<promise_type> handle;
            bool await_ready() const noexcept { return !handle || handle.done(); }
            auto await_suspend(std::coroutine_handle<> awaiting) const noexcept {
                if (handle) handle.promise().continuation = awaiting;
                return handle;
            }
            auto await_resume() const requires (!Void<T>) {
                return std::move(handle.promise()).consume_value_or_exception();
            }
            void await_resume() const requires Void<T> {
                handle.promise().rethrow_exception();
            }
            ~Awaiter() {
                if (handle) handle.destroy();
            }
        };
        auto h = this->handle;
        this->handle = nullptr;
        return Awaiter{h};
    }
};

template <typename T>
using Eager = EagerTask<T>;

template <typename T>
using Lazy = LazyTask<T>;

}
LCORE_NAMESPACE_END
