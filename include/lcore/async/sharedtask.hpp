#pragma once
#include "lcore/assert.hpp"
#include "lcore/async/traits.hpp"
#include "sharedpromise.hpp"
#include "lcore/exception.hpp"
#include "lcore/memory.hpp"
#include "task.hpp"
#include <coroutine>
#include <mutex>

LCORE_NAMESPACE_BEGIN
namespace async {

class EmptyStateException: public Exception {
    const char* what() const noexcept override {
        return "State is empty";
    }
};

namespace _detail {
struct StateBase {
    std::coroutine_handle<PromiseBase> handle;
    mutable std::mutex mutex;

    StateBase() = default;
    StateBase(std::coroutine_handle<PromiseBase> handle): handle(handle) {
    }
    StateBase(const StateBase&) = delete;
    StateBase(StateBase&& other) {
        std::lock_guard<std::mutex> lock(other.mutex);
        handle = other.handle;
        other.handle = nullptr;
    }
    ~StateBase() {
#ifdef LCORE_DEBUG
        if (handle && !handle.done()) {
            LCORE_WARN("Destroying a Task whose coroutine is not finished. This may cause resource leaks or undefined behavior.");
        }
#endif
        if (handle) handle.destroy();
    }

    StateBase& operator=(const StateBase&) = delete;
    StateBase& operator=(StateBase&& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mutex);
            std::lock_guard<std::mutex> lock2(other.mutex);
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    bool has_handle() const {
        return handle != nullptr;
    }
    bool done() const {
        return !handle || handle.done();
    }
    void destroy() {
        if (handle) {
            handle.destroy();
            handle = nullptr;
        }
    }
    void resume() {
        if (handle) handle.resume();
    }
    bool has_exception() const {
        if(handle) return handle.promise().has_exception();
        return false;
    }
    std::exception_ptr get_exception() const {
        if(handle) return handle.promise().get_exception();
        return nullptr;
    }
};



template <typename T, IsAwaitableImplement InitialSuspend, template <typename> typename TaskWrapper>
struct State: public StateBase {
    using PromiseType = SharedPromise<T, InitialSuspend, TaskWrapper>;

    State() = default;
    State(std::coroutine_handle<PromiseType> handle): 
        StateBase(std::coroutine_handle<PromiseBase>::from_address(handle.address())) {
    }

    std::coroutine_handle<PromiseType> get_handle() const {
        return std::coroutine_handle<PromiseType>::from_address(handle.address());
    }
    bool has_value() const requires (!Void<T>) {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) return get_handle().promise().has_value();
        return false;
    }
    T consume_value() && requires MoveConstructible<T> {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) return std::move(get_handle().promise()).consume_value_or_exception();
        throw EmptyStateException();
    }
    T peek_value() const & requires CopyConstructible<T> {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) return get_handle().promise().peek_value_or_exception();
        throw EmptyStateException();
    }
    auto& ref_value() const & requires (!CopyConstructible<T>) {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) return get_handle().promise().ref_value_or_exception();
        throw EmptyStateException();
    }
    std::exception_ptr get_exception() const {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) return get_handle().promise().get_exception();
        return nullptr;
    }
    void rethrow_exception() const {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) get_handle().promise().rethrow_exception();
    }
    bool has_exception() const {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) return get_handle().promise().has_exception();
        return false;
    }
    void wait(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(mutex);
        if(handle) get_handle().promise().add_waiter(h);
    }
};
}

template <typename T, IsAwaitableImplement InitialSuspend, template <typename> typename TaskWrapper>
class SharedTaskBase {
public:
    using StateType = _detail::State<T, InitialSuspend, TaskWrapper>;
    using promise_type = typename StateType::PromiseType;
    struct sentinel{};
protected:
    Ptr<StateType> state;
public:
    SharedTaskBase(): state(nullptr) {
    }
    SharedTaskBase(std::coroutine_handle<promise_type> handle): state(New<StateType>(handle)) {
    }
    SharedTaskBase(const SharedTaskBase&) = default;
    SharedTaskBase(SharedTaskBase&&) = default;
    SharedTaskBase& operator=(const SharedTaskBase&) = default;
    SharedTaskBase& operator=(SharedTaskBase&&) = default;

    std::coroutine_handle<promise_type> get_handle() const {
        if (state) return state->get_handle();
        return nullptr;
    }
    operator std::coroutine_handle<>() const {
        if (state) return state->get_handle();
        return nullptr;
    }
    Ptr<StateType> get_state() const {
        return state;
    }
    
    bool done() const { return state->done(); }
    void resume() { if(state) state->resume(); }
    void destroy() { if(state) state->destroy(); }
    bool has_exception() const { return state->has_exception(); }
    std::exception_ptr get_exception() const { return state->get_exception(); }
    bool has_value() const requires (!Void<T>) {
        return !state->is_exception() && state->done();
    }
    T peek_value() const requires (!Void<T>) { return state->peek_value(); }
    T consume_value() &&  requires (!Void<T>) { return std::move(*state).consume_value(); }
    auto& ref_value() const requires (!Void<T>) { return state->ref_value(); }
};

template <typename T = void>
class SharedEagerTask: public SharedTaskBase<T, std::suspend_never, SharedEagerTask> {
    using Base = SharedTaskBase<T, std::suspend_never, SharedEagerTask>;
    using StateType = typename Base::StateType;
public:
    using Base::Base;
    using promise_type = typename Base::promise_type;

    auto operator co_await() const {
        struct Awaiter {
            Ptr<StateType> state;
            bool await_ready() const noexcept {
                return state->done();
            }
            void await_suspend(std::coroutine_handle<> h) {
                state->wait(h);
            }
            auto await_resume() const requires (!Void<T>) {
                state->rethrow_exception();
                return state->peek_value();
            }
            void await_resume() const requires Void<T> {
                state->rethrow_exception();
            }
        };
        return Awaiter{this->state};
    }
};

template <typename T = void>
class SharedLazyTask: public SharedTaskBase<T, std::suspend_always, SharedLazyTask> {
    using Base = SharedTaskBase<T, std::suspend_always, SharedLazyTask>;
    using StateType = typename Base::StateType;
public:
    using Base::Base;
    using promise_type = typename Base::promise_type;

    auto operator co_await() const & {
        struct Awaiter {
            Ptr<StateType> state;
            bool await_ready() const noexcept {
                return state->done();
            }
            void await_suspend(std::coroutine_handle<> h) {
                state->wait(h);
            }
            auto await_resume() const requires (!Void<T>) {
                state->rethrow_exception();
                return state->peek_value();
            }
            void await_resume() const requires Void<T> {
                state->rethrow_exception();
            }
        };
        return Awaiter{this->state};
    }
    auto operator co_await() && {
        struct Awaiter {
            Ptr<StateType> state;
            bool await_ready() const noexcept {
                return state->done();
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
                state->wait(h);
                if (state.UseCount() == 1) {
                    return state->get_handle();
                } else {
                    return {};
                }
            }
            auto await_resume() const requires (!Void<T>) {
                state->rethrow_exception();
                return std::move(*state).consume_value();
            }
            void await_resume() const requires Void<T> {
                state->rethrow_exception();
            }
        };
        return Awaiter{std::move(this->state)};
    }
};

template <typename T = void>
using SharedLazy = SharedLazyTask<T>;

template <typename T = void>
using SharedEager = SharedEagerTask<T>;

}
LCORE_NAMESPACE_END
