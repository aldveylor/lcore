#pragma once
#include "lcore/base.hpp"
#include "lcore/traits.hpp"
#include <coroutine>

LCORE_NAMESPACE_BEGIN

// Awaitable

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
concept HasCoAwaitOperator = requires(T t){
    {t.operator co_await()} -> IsAwaitableImplement;
} || requires(T t){
    {std::move(t).operator co_await()} -> IsAwaitableImplement;
};

template <typename T>
concept IsAwaitable = HasCoAwaitOperator<T> || requires (T t){
    {operator co_await(t)} -> IsAwaitableImplement;
};

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

namespace _detail {

// 1. member operator co_await
template<class T>
auto get_awaiter_impl(T&& t, int)
    -> decltype(std::forward<T>(t).operator co_await())
{
    return std::forward<T>(t).operator co_await();
}

// 2. ADL operator co_await
template<class T>
auto get_awaiter_impl(T&& t, long)
    -> decltype(operator co_await(std::forward<T>(t)))
{
    return operator co_await(std::forward<T>(t));
}

// 3. identity awaiter (already awaiter)
template<class T>
auto get_awaiter_impl(T&& t, long long)
    -> T&&;

template<class T>
decltype(auto) get_awaiter(T&& t)
{
    return get_awaiter_impl(std::forward<T>(t), 0);
}

// ---------- 2. awaiter type ----------
template<class T>
using awaiter_t =
    std::remove_reference_t<
        decltype(_detail::get_awaiter(std::declval<T>()))
    >;

// ---------- 3. result type ----------
template<class T>
using await_result_t =
    decltype(std::declval<awaiter_t<T>&>().await_resume());


} // namespace _detail

template <typename T>
using GetAwaiter = _detail::awaiter_t<T>;

template <typename T>
using GetAwaitResult = _detail::await_result_t<T>;

LCORE_NAMESPACE_END
