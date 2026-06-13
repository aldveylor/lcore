#pragma once
#include "promise.hpp"
#include <vector>

LCORE_ASYNC_NAMESPACE_BEGIN

template <typename T, IsAwaitableImplement InitialAwaitable, template <typename> typename TaskWrapper>
class SharedPromise: public PromiseBase, public SuspendHandler<InitialAwaitable> {
public:
    using value_type = T;
    using suspend_handler_type = SuspendHandler<InitialAwaitable>;
    using promise_type = SharedPromise<T, InitialAwaitable, TaskWrapper>;
    using handle_type = std::coroutine_handle<promise_type>;

    SharedPromise() = default;
    ~SharedPromise() = default;

    TaskWrapper<T> get_return_object(){
        return TaskWrapper<T>(handle_type::from_promise(*this));
    }

    struct final_awaiter {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
            auto& p = h.promise();
            for (auto& cont : p.continuations) {
                cont.resume();
            }
        }
        void await_resume() noexcept {}
    };
    auto final_suspend() noexcept { return final_awaiter{}; }

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

    void add_waiter(std::coroutine_handle<> cont) {
        this->continuations.push_back(cont);
    }

    std::vector<std::coroutine_handle<>> continuations;
private:
    std::optional<T> value;
};

template <IsAwaitableImplement InitialAwaitable, template <typename> typename TaskWrapper>
class SharedPromise<void, InitialAwaitable, TaskWrapper>: public PromiseBase, public SuspendHandler<InitialAwaitable> {
public:
    using value_type = void;
    using promise_type = SharedPromise<void, InitialAwaitable, TaskWrapper>;
    using handle_type = std::coroutine_handle<promise_type>;

    SharedPromise() = default;
    ~SharedPromise() = default;

    TaskWrapper<void> get_return_object(){
        return TaskWrapper<void>(handle_type::from_promise(*this));
    }

    struct final_awaiter {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
            auto& p = h.promise();
            for (auto& cont : p.continuations) {
                cont.resume();
            }
        }
        void await_resume() noexcept {}
    };
    auto final_suspend() noexcept { return final_awaiter{}; }

    void return_void(){
    }

    void set_exception(std::exception_ptr exception) noexcept {
        PromiseBase::set_exception(exception);
    }

    void rethrow_exception() const {
        if(this->exception) std::rethrow_exception(this->exception);
    }

    void add_waiter(std::coroutine_handle<> cont) {
        this->continuations.push_back(cont);
    }

    std::vector<std::coroutine_handle<>> continuations;
};

LCORE_ASYNC_NAMESPACE_END

