#pragma once
#include "base.hpp"
#include "traits.hpp"
#include "executor.hpp"
#include <exception>
#include <optional>
#include <coroutine>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <vector>

LCORE_NAMESPACE_BEGIN
namespace async {
namespace _detail {

template <typename Starter, typename... Args>
struct CallbackAwaiter {
    using tuple_t = std::tuple<std::decay_t<Args>...>;

    Starter starter;
    alignas(tuple_t) unsigned char storage[sizeof(tuple_t)] = {};
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

namespace _detail {

    
template <typename T>
bool atomic_set_if_not(std::atomic<T>& atomic, T bad, T value) {
    T current = atomic.load(std::memory_order_relaxed);
    while (current != bad) {
        if (atomic.compare_exchange_weak(current, value, std::memory_order_release, std::memory_order_relaxed)) {
            return true;
        }
        // Failed to set, current is updated with the latest value of atomic, check if it's still not bad
    }
    return false; // atomic already has the bad value, failed to set
};

template <typename... Awaiter>
struct WhenAllAwaiterWrapper {
    using ResultTuple = std::tuple<ReplaceIf<GetAwaitResult<Awaiter>, void, Monostate>...>;
    using StroagedResultTuple = std::tuple<std::optional<GetAwaitResult<Awaiter>>...>;

    std::mutex mutex;   // For mutiple threads safety
    std::tuple<Awaiter...> awaiters;
    std::tuple<std::optional<ReplaceIf<GetAwaitResult<Awaiter>, void, Monostate>>...> results;
    std::exception_ptr exception;
    std::atomic<std::size_t> count{ sizeof...(Awaiter) };
    std::coroutine_handle<> handle;
    Scheduler& scheduler = Scheduler::GetInstance();

    WhenAllAwaiterWrapper(Awaiter&&... awts) : awaiters(std::move(awts)...) {}

    template<std::size_t... I, class... Awts>
    void schedule_all(std::index_sequence<I...>, Awts&&... awts){
        (schedule_one<I>(std::forward<Awts>(awts)), ...);
    }

    template<std::size_t I, class A>
    void schedule_one(A&& awt)
    {
        scheduler.Schedule([this](A&& awt) mutable -> Lazy<void> {
            try {
                if constexpr (std::is_void_v<GetAwaitResult<std::decay_t<A>>>) {
                    co_await std::move(awt);
                    std::lock_guard lock(mutex);
                    std::get<I>(results) = Monostate{};
                } else {
                    auto res = co_await std::move(awt);
                    std::lock_guard lock(mutex);
                    std::get<I>(results) = std::move(res);
                }
            } catch (...) {
                {
                   std::lock_guard lock(mutex);
                    if (exception) co_return; // An exception has already been recorded, no need to resume again
                    exception = std::current_exception();
                }
                handle.resume();
                co_return;
            }
            if (--count == 0 && !exception) {
                handle.resume();
            }
        }(std::forward<A>(awt)));
    }

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        this->handle = h;
        std::apply([this](auto&&... awts) {
            using IS = std::index_sequence_for<Awaiter...>;
            schedule_all(IS{}, std::forward<decltype(awts)>(awts)...);
        }, awaiters);
    }
    auto await_resume() {
        if (exception) std::rethrow_exception(exception);
        return std::apply([](auto&&... res) {
            return std::make_tuple(std::move(*res)...);
        }, results);
    }
};

template <typename... Awaiter>
struct WhenAnyAwaiterWrapper {
    std::tuple<Awaiter...> awaiters;
    std::tuple<std::optional<ReplaceIf<GetAwaitResult<Awaiter>, void, Monostate>>...> results;
    // Use shared_ptr to avoid the lifetime issue of WhenAnyAwaiterWrapper
    // WhenAnyAwaiterWrapper may be destroyed when some of the awaiters is still running
    // but the ready_index and exception must be valid until the coroutine is resumed
    std::shared_ptr<std::atomic<int>> ready_index = std::make_shared<std::atomic<int>>(-1);
    std::exception_ptr exception;
    std::coroutine_handle<> handle;
    Scheduler& scheduler = Scheduler::GetInstance();

    WhenAnyAwaiterWrapper(Awaiter&&... awts) : awaiters(std::move(awts)...) {}
    template<std::size_t... I, class... Awts>
    void schedule_all(std::index_sequence<I...>, Awts&&... awts){
        (schedule_one<I>(std::forward<Awts>(awts)), ...);
    }
    template<std::size_t I, class A>
    void schedule_one(A&& awt)
    {
        scheduler.Schedule([this](A&& awt) mutable -> Lazy<void> {
            try {
                if constexpr (std::is_void_v<GetAwaitResult<std::decay_t<A>>>) {
                    co_await std::move(awt);
                    std::get<I>(results) = Monostate{};
                } else {
                    std::get<I>(results) = co_await std::move(awt);
                }
            } catch (...) {
                // This object is only meaningful when ready_index is -1
                // When it's set to a non-negative value, it means `co_await` has been already resumed,
                // and this awaiter may be destroyed

                int expected = -1;
                if (ready_index->compare_exchange_strong(expected, I)) {
                    exception = std::current_exception();
                    handle.resume();
                }
                co_return;
            }
            int expected = -1;
            if (ready_index->compare_exchange_strong(expected, I)) {
                handle.resume();
            }
        }(std::forward<A>(awt)));
    }
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        this->handle = h;
        std::apply([this](auto&&... awts) {
            using IS = std::index_sequence_for<Awaiter...>;
            schedule_all(IS{}, std::forward<decltype(awts)>(awts)...);
        }, awaiters);
    }
    auto await_resume() {
        if (exception) std::rethrow_exception(exception);
        int index = ready_index->load();
        if (index < 0) throw RuntimeError("No awaiter is ready");
        return std::make_pair(index, std::move(this->results));
    }
};

}

/// @brief Waits for all awaitables to complete and returns their results as a tuple.
/// **Note**: The coroutines will be resumed in the same thread that completes the last awaitable.
template <typename ...Awaiter>
requires (IsAwaitable<Awaiter> && ...)
inline auto WhenAll(Awaiter&&... awaiters) {
    return _detail::WhenAllAwaiterWrapper(std::move(awaiters)...);
};

/// @brief Waits for any one of the awaitables to complete and returns a pair of the index of the completed awaitable and its result.
/// **Note**: The coroutine will be resumed in the same thread that completes the first await
template <typename ...Awaiter>
requires (IsAwaitable<Awaiter> && ...)
inline auto WhenAny(Awaiter&&... awaiters) {
    return _detail::WhenAnyAwaiterWrapper(std::move(awaiters)...);
};

template <Iterable AwaitableContainer>
requires (IsAwaitable<typename std::decay_t<AwaitableContainer>::value_type>)
inline auto WhenAll(AwaitableContainer&& container) {
    using AwaiterType = typename std::decay_t<AwaitableContainer>::value_type;
    using ResultType = GetAwaitResult<AwaiterType>;
    struct AwaiterNotVoid {
        std::mutex mutex;
        AwaitableContainer awaitables;
        std::vector<std::optional<ResultType>> results;
        std::atomic<size_t> count;
        std::exception_ptr exception;
        std::coroutine_handle<> handle;
        Scheduler& scheduler = Scheduler::GetInstance();

        AwaiterNotVoid(AwaitableContainer&& cont) : awaitables(std::move(cont)), results(awaitables.size()), count(awaitables.size()) {}
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            this->handle = h;
            for (size_t i = 0; i < awaitables.size(); ++i) {
                scheduler.Schedule([this, i](AwaiterType awt) mutable -> Lazy<void> {
                    try {
                        auto res = co_await std::move(awt);
                        std::lock_guard lock(mutex);
                        results[i] = std::move(res);
                    } catch (...) {
                        {
                            std::lock_guard lock(mutex);
                            if (exception) co_return; // An exception has already been recorded, no need to resume again
                            exception = std::current_exception();
                        }
                        handle.resume();
                        co_return;
                    }
                    if (--count == 0 && !exception) {
                        handle.resume();
                    }
                }(std::move(awaitables[i])));
            }
        }
        auto await_resume() {
            if (exception) std::rethrow_exception(exception);
            std::vector<ResultType> ret;
            for (auto& res : results) {
                ret.push_back(std::move(*res));
            }
            return ret;
        }
    };
    struct AwaiterVoid {
        std::mutex mutex;
        AwaitableContainer awaitables;
        std::atomic<size_t> count;
        std::coroutine_handle<> handle;
        std::exception_ptr exception;
        Scheduler& scheduler = Scheduler::GetInstance();

        AwaiterVoid(AwaitableContainer&& cont) : awaitables(std::move(cont)), count(awaitables.size()) {}
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            this->handle = h;
            for (size_t i = 0; i < awaitables.size(); ++i) {
                scheduler.Schedule([this, i](AwaiterType awt) mutable -> Lazy<void> {
                    try {
                        co_await std::move(awt);
                    } catch (...) {
                        {
                            std::lock_guard lock(mutex);
                            if (exception) co_return; // An exception has already been recorded, no need to
                            this->exception = std::current_exception();
                        }
                        handle.resume();
                        co_return;
                    }
                    if (--count == 0 && !exception) {
                        handle.resume();
                    }
                }(std::move(awaitables[i])));
            }
        }
        void await_resume() const noexcept { if (exception) std::rethrow_exception(exception); }
    };
    if constexpr (Void<ResultType>) {
        return AwaiterVoid(std::forward<AwaitableContainer>(container));
    } else {
        return AwaiterNotVoid(std::forward<AwaitableContainer>(container));
    }
};

template <Iterable AwaitableContainer>
requires (IsAwaitable<typename std::decay_t<AwaitableContainer>::value_type>)
inline auto WhenAny(AwaitableContainer&& container) {
    using AwaiterType = typename std::decay_t<AwaitableContainer>::value_type;
    using ResultType = GetAwaitResult<AwaiterType>;
    struct AwaiterNotVoid {
        AwaitableContainer awaitables;
        std::vector<std::optional<ResultType>> results;
        std::shared_ptr<std::atomic<int>> ready_index = std::make_shared<std::atomic<int>>(-1);
        std::exception_ptr exception;
        std::coroutine_handle<> handle;
        Scheduler& scheduler = Scheduler::GetInstance();

        AwaiterNotVoid(AwaitableContainer&& cont) : awaitables(std::move(cont)), results(awaitables.size()) {}
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            this->handle = h;
            for (size_t i = 0; i < awaitables.size(); ++i) {
                scheduler.Schedule([this, i](AwaiterType awt) mutable -> Lazy<void> {
                    try {
                        results[i] = co_await std::move(awt); // Setting different index won't cause data race
                    } catch (...) {
                        int expected = -1;
                        if (ready_index->compare_exchange_strong(expected, i)) {
                            exception = std::current_exception();
                            handle.resume();
                        }
                        co_return;
                    }
                    int expected = -1;
                    if (ready_index->compare_exchange_strong(expected, i)) {
                        handle.resume();
                    }
                }(std::move(awaitables[i])));
            }
        }
        auto await_resume() {
            if (exception) std::rethrow_exception(exception);
            int index = ready_index->load();
            if (index < 0) throw RuntimeError("No awaiter is ready");
            return std::make_pair(index, std::move(this->results[index]));
        }
    };
    struct AwaiterVoid {
        AwaitableContainer awaitables;
        std::shared_ptr<std::atomic<int>> ready_index = std::make_shared<std::atomic<int>>(-1);
        std::exception_ptr exception;
        std::coroutine_handle<> handle;
        Scheduler& scheduler = Scheduler::GetInstance();

        AwaiterVoid(AwaitableContainer&& cont) : awaitables(std::move(cont)) {}
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            this->handle = h;
            for (size_t i = 0; i < awaitables.size(); ++i) {
                scheduler.Schedule([this, i](AwaiterType awt) mutable -> Lazy<void> {
                    try {
                        co_await std::move(awt);
                    } catch (...) {
                        int expected = -1;
                        if (ready_index->compare_exchange_strong(expected, i)) {
                            exception = std::current_exception();
                            handle.resume();
                        }
                        co_return;
                    }
                    int expected = -1;
                    if (ready_index->compare_exchange_strong(expected, i)) {
                        handle.resume();
                    }
                }(std::move(awaitables[i])));
            }
        }
        auto await_resume() {
            if (exception) std::rethrow_exception(exception);
            int index = ready_index->load();
            if (index < 0) throw RuntimeError("No awaiter is ready");
            return index;
        }
    };
    if constexpr (Void<ResultType>) {
        return AwaiterVoid(std::forward<AwaitableContainer>(container));
    } else {
        return AwaiterNotVoid(std::forward<AwaitableContainer>(container));
    }
};

}
LCORE_NAMESPACE_END
