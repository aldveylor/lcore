#pragma once
#include "lcore/base.hpp"
#include "lcore/traits.hpp"
#include <coroutine>

LCORE_NAMESPACE_BEGIN

// Awaitable

template <typename T>
concept HasCoAwaitOperator = requires(T t){
    {t.operator co_await()} -> Same<std::suspend_always>;
};

template <typename T>
concept IsCoroutineHandle = requires(T t){
    {t.resume()};
    {t.done()} -> Same<bool>;
    {t.destroy()};
};

namespace _detail {
    template <typename T>
    concept AwaitSuspendReturnType = Same<T, void> || Same<T, bool> || IsCoroutineHandle<T>;
};

template <typename T>
concept IsAwaitableImplement = requires(T t){
    // typename T::value_type;
    {t.await_ready()} -> ConvertibleTo<bool>;

    // await_suspend has a template parameter, so we can't check the return type directly.
    // {t.await_suspend(std::coroutine_handle<>{})} -> _detail::AwaitSuspendReturnType;
    
    {t.await_resume()};
};

template <typename T>
concept IsAwaitable = HasCoAwaitOperator<T> || IsAwaitableImplement<T>;

// Promise like

template <typename T>
concept IsSuspendHandler = requires(T t){
    {t.initial_suspend()} -> IsAwaitableImplement;
    {t.final_suspend()} -> IsAwaitableImplement;
};

template <typename T>
concept PromiseReturnValue = requires(T t){
    {t.return_value(std::declval<typename T::value_type>())};
};

template <typename T>
concept PromiseReturnVoid = requires(T t){
    {t.return_void()};
};

template <typename T>
concept PromiseYieldValue = requires(T t){
    {t.yield_value(std::declval<typename T::value_type>())};
};

template <typename T>
concept IsPromise = requires(T t){
    requires IsSuspendHandler<T>;
    requires PromiseReturnValue<T> || PromiseReturnVoid<T> || PromiseYieldValue<T>;
    // {t.get_return_object()};
    {t.unhandled_exception()};
};

// Task like

template <typename T>
concept IsTask = requires(T t){
    typename T::promise_type;
    requires IsPromise<typename T::promise_type>;
    requires ConstructibleWith<T, std::coroutine_handle<typename T::promise_type>>;
};

LCORE_NAMESPACE_END
