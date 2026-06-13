#pragma once
#include "base.hpp"
#include "traits.hpp"
#include "task.hpp"
#include <exception>

LCORE_ASYNC_NAMESPACE_BEGIN

/// @brief An asynchronous context that can be entered with some initialization arguments and a function to execute within the context.
/// @tparam InitRet The type of the result returned by the initialization logic.
/// @tparam InitArgs The types of the arguments passed to the initialization logic.
/// Example usage:
/// ```cpp
/// class MyContext : public AsyncContext<int, std::string> {
/// protected:
///     Lazy<int> DoInit(std::string&& arg) override {
///         // Perform initialization logic here, such as allocating resources or setting up state.
///         co_return 42; // Return an initialization result that will be passed to the function
///     }
///     Lazy<bool> DoFinal(std::exception_ptr eptr) override {
///         // Perform finalization logic here, such as cleaning up resources or handling errors.
///         if (eptr) {
///             // Handle the exception if needed, such as logging or cleanup.
///             return false; // Indicate that the exception has been handled and should not be rethrown.
///         }
///         return true;
///     }
/// };
/// 
/// static MyContext context;
/// context.Enter("Initialization argument", [](int initResult) -> Lazy<void> {
///     // This function will be executed within the context, with access to the initialization result.
///     // Perform the main logic of the function here, using the initialization result as needed.
///     co_return; // Return when the function is complete. Any exceptions thrown here will be caught and passed to the finalization logic.
/// });
template <typename InitRet = void, typename ...InitArgs>
class AsyncContext: public AbstractClass {
protected:
    virtual Lazy<InitRet> DoInit(InitArgs&& ...args) = 0;
    virtual Lazy<bool> DoFinal(std::exception_ptr) = 0;
public:
    template <typename Func>
    requires (!Void<InitRet>) && IsAwaitable<ResultCallable<Func, InitRet>>
    Lazy<void> Enter(InitArgs&& ...args, Func func){
        auto result = co_await DoInit(std::forward<InitArgs>(args)...);
        std::exception_ptr eptr;
        try {
            co_await func(result);
        } catch (...) {
            eptr = std::current_exception();
        }
        if (co_await DoFinal(eptr) && eptr) {
            throw eptr;
        }
    }
    template <typename Func>
    requires Void<InitRet> && IsAwaitable<ResultCallable<Func>>
    Lazy<void> Enter(InitArgs&& ...args, Func func){
        co_await DoInit(std::forward<InitArgs>(args)...);
        std::exception_ptr eptr;
        try {
            co_await func();
        } catch (...) {
            eptr = std::current_exception();
        }
        if (co_await DoFinal(eptr) && eptr) {
            throw eptr;
        }
    }
};

LCORE_ASYNC_NAMESPACE_END
