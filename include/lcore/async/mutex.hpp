#pragma once
#include "base.hpp"
#include "lcore/exception.hpp"
#include <coroutine>
#include <queue>
#include <optional>
#include <mutex>

LCORE_NAMESPACE_BEGIN
namespace async {

class AsyncMutex {
    bool m_locked = false;
    std::queue<std::coroutine_handle<>> m_waiters;
    mutable std::mutex m_mutex; // Protects access to m_waiters
public:
    struct LockAwaiter {
        AsyncMutex& m_mutex;
        bool await_ready() const noexcept { return !m_mutex.m_locked; }
        void await_suspend(std::coroutine_handle<> handle) {
            std::lock_guard<std::mutex> lock(m_mutex.m_mutex);
            m_mutex.m_waiters.push(handle);
        }
        void await_resume() noexcept {}
    };
    LockAwaiter lock() noexcept {
        return LockAwaiter{*this};
    }
    void unlock() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_waiters.empty()) {
            m_locked = false;
        } else {
            auto handle = m_waiters.front();
            m_waiters.pop();
            handle.resume();
        }
    }
    std::optional<LockAwaiter> try_lock() noexcept {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_locked) return std::nullopt;
            m_locked = true;
        }
        return LockAwaiter{*this};
    }
    bool is_locked() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_locked;
    }
};

/// @brief RAII guard for AsyncMutex
/// Usage:
/// ```cpp
/// AsyncMutex mutex;
/// AsyncLockGuard guard(mutex);
/// co_await guard; // Locks the mutex
/// // Critical section
/// // Mutex will be automatically released when guard goes out of scope
/// ```
template <typename MutexType = AsyncMutex>
class AsyncLockGuard {
    MutexType& m_mutex;
public:
    explicit AsyncLockGuard(MutexType& mutex) : m_mutex(mutex){};
    ~AsyncLockGuard() { m_mutex.unlock(); }
    auto operator co_await() const & noexcept {
        return m_mutex.lock();
    }
};

template <typename MutexType = AsyncMutex>
class AsyncUniqueLock {
    MutexType& m_mutex;
    bool m_owns_lock = false;
public:
    explicit AsyncUniqueLock(MutexType& mutex) : m_mutex(mutex){};
    ~AsyncUniqueLock() {
        if (m_owns_lock) {
            m_mutex.unlock();
        }
    }
    auto operator co_await() & noexcept {
        if (m_owns_lock) {
            throw LogicError("Lock already owned");
        }
        m_owns_lock = true;
        return m_mutex.lock();
    }
    void unlock() {
        if (!m_owns_lock) {
            throw LogicError("Lock not owned");
        }
        m_mutex.unlock();
        m_owns_lock = false;
    }
    bool owns_lock() const noexcept {
        return m_owns_lock;
    }
};

} // namespace async
LCORE_NAMESPACE_END
