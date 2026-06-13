#pragma once
#include "base.hpp"
#include "lcore/traits.hpp"
#include <optional>
#include <coroutine>
#include <mutex>
#include <condition_variable>

LCORE_NAMESPACE_BEGIN
namespace async {
namespace _detail {

template <typename Starter, typename... Args>
struct CallbackAwaiter {
    using tuple_t = std::tuple<std::decay_t<Args>...>;

    Starter starter;
    alignas(tuple_t) unsigned char storage[sizeof(tuple_t)];
    bool has_value = false;

    bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        starter([this, h](Args&&... values) {
            ::new (storage) tuple_t(std::forward<Args>(values)...);
            has_value = true;
            h.resume();
        });
    }

    auto await_resume() {
        auto* tup = reinterpret_cast<tuple_t*>(storage);
        if constexpr (sizeof...(Args) == 1) {
            auto&& val = std::get<0>(*tup);
            using RetT = std::tuple_element_t<0, tuple_t>;
            RetT ret = std::move(val);
            tup->~tuple_t();
            return ret;
        } else {
            tuple_t ret = std::move(*tup);
            tup->~tuple_t();
            return ret;
        }
    }
};

template <typename Starter, typename ArgTuple>
struct _CallbackAwaiterHelper;

template <typename Starter, typename... Args>
struct _CallbackAwaiterHelper<Starter, std::tuple<Args...>> {
    using type = CallbackAwaiter<Starter, Args...>;
};

template <typename Starter, typename ArgTuple>
using _CallbackAwaiterHelper_t = typename _CallbackAwaiterHelper<Starter, ArgTuple>::type;

}

/// @brief Creates an awaiter that wraps a callback-based asynchronous operation.
/// **Note**: The coroutine will be resumed in the same thread that calls the callback.
/// Example usage:
/// ```cpp
/// auto result = co_await MakeCallbackAwaiter([](auto callback) {
///     // Simulate an asynchronous operation
///     std::thread([callback] {
///         std::this_thread::sleep_for(std::chrono::seconds(1));
///         callback(42); // Invoke the callback with the result
///     }).detach();
/// });
/// std::cout << "Result: " << result << std::endl; // Output: Result: 42
/// ```
template <typename Starter>
auto MakeCallbackAwaiter(Starter&& starter) {
    using Callback = NthTypeOfTuple<0, typename FunctionTraits<Starter>::ArgsTuple>;
    using Args = typename FunctionTraits<Callback>::ArgsTuple;
    if constexpr (std::is_function_v<Starter>)
        return _detail::_CallbackAwaiterHelper_t<Starter*, Args>{ std::forward<Starter>(starter) };
    else return _detail::_CallbackAwaiterHelper_t<Starter, Args>{ std::forward<Starter>(starter) };
}

/// @brief An awaitable that can be manually triggered to resume the awaiting coroutine.
/// **Note**: resume will happened in the same thread that calls trigger.
class TriggedAwaitable {
    std::coroutine_handle<> m_handle;
    std::mutex m_mutex;
    std::condition_variable m_cv;
public:
    TriggedAwaitable() = default;

    bool await_ready() const noexcept {
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle) noexcept {
        m_handle = handle;
        m_cv.notify_all();
    }
    void await_resume() const noexcept {}

    bool trigger() noexcept {
        if (m_handle) {
            auto h = m_handle;
            m_handle = nullptr;
            h.resume();
            return true;
        }
        return false;
    }
    bool has_handle() const noexcept {
        return m_handle != nullptr;
    }
    void wait_for_handle() {
        if (m_handle) return;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_handle != nullptr; });
    }
};

}
LCORE_NAMESPACE_END
