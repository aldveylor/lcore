#pragma once
#include "base.hpp"
#include "lcore/class.hpp"
#include <coroutine>
#include <exception>
#include <utility>
#include <optional>
#include "traits.hpp"

LCORE_ASYNC_NAMESPACE_BEGIN

template <typename InitSuspend = std::suspend_never, typename FinalSuspend = std::suspend_always>
class SuspendHandler {
public:
    using InitSuspendType = InitSuspend;
    using FinalSuspendType = FinalSuspend;

    auto initial_suspend() noexcept { return InitSuspend();}
    auto final_suspend() noexcept { return FinalSuspend(); }
};

template <typename T, IsAwaitableImplement InitialAwaitable, template <typename> typename TaskWrapper>
class Promise;

class PromiseBase {
protected:
    std::exception_ptr exception;
    
    void set_exception(std::exception_ptr exception) noexcept {
        this->exception = exception;
    }
public:
    void unhandled_exception() noexcept {
        this->exception = std::current_exception();
    }

    std::exception_ptr get_exception() const {
        return this->exception;
    }

    bool has_exception() const {
        return this->exception != nullptr;
    }

    void rethrow_exception() const {
        if(this->exception) std::rethrow_exception(this->exception);
    }
};

template <typename T, IsAwaitableImplement InitialAwaitable, template <typename> typename TaskWrapper>
class Promise: public PromiseBase, public SuspendHandler<InitialAwaitable> {
public:
    using value_type = T;
    using suspend_handler_type = SuspendHandler<InitialAwaitable>;
    using promise_type = Promise<T, InitialAwaitable, TaskWrapper>;
    using handle_type = std::coroutine_handle<promise_type>;

    Promise() = default;
    ~Promise() = default;

    TaskWrapper<T> get_return_object(){
        return TaskWrapper<T>(handle_type::from_promise(*this));
    }

    auto final_suspend() noexcept { 
        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                auto& p = h.promise();
                if (p.continuation) p.continuation.resume();
            }
            void await_resume() noexcept {}
        };    
        return final_awaiter{}; 
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
        PromiseBase::set_exception(exception);
        this->value.reset();
    }

    const T& ref_value_or_exception() const & {
        this->rethrow_exception();
        return this->value.value();
    }

    T peek_value_or_exception() const requires CopyConstructible<T> {
        this->rethrow_exception();
        return this->value.value();
    }

    T consume_value_or_exception() && requires MoveConstructible<T> {
        this->rethrow_exception();
        return std::move(this->value.value());
    }

    bool has_value() const {
        return this->value.has_value();
    }

    const std::optional<T>& get_value_opt() const {
        return this->value;
    }

    std::coroutine_handle<> continuation{};
private:
    std::optional<T> value;
};

template <IsAwaitableImplement InitialAwaitable, template <typename> typename TaskWrapper>
class Promise<void, InitialAwaitable, TaskWrapper>: public PromiseBase, public SuspendHandler<InitialAwaitable> {
public:
    using value_type = void;
    using promise_type = Promise<void, InitialAwaitable, TaskWrapper>;
    using handle_type = std::coroutine_handle<promise_type>;

    Promise() = default;
    ~Promise() = default;

    TaskWrapper<void> get_return_object(){
        return TaskWrapper<void>(handle_type::from_promise(*this));
    }

    auto final_suspend() noexcept { 
        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                auto& p = h.promise();
                if (p.continuation) p.continuation.resume();
            }
            void await_resume() noexcept {}
        };
        return final_awaiter{}; 
    }

    void return_void(){
    }

    void set_exception(std::exception_ptr exception) noexcept {
        PromiseBase::set_exception(exception);
    }

    void rethrow_exception() const {
        if(this->exception) std::rethrow_exception(this->exception);
    }

    std::coroutine_handle<> continuation{};
};

LCORE_ASYNC_NAMESPACE_END

