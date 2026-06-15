#pragma once
#include "mutex.hpp"

LCORE_NAMESPACE_BEGIN
namespace async {

class SharedMutex {
    bool exclusive = false;
    size_t shared_count = 0;
    std::deque<std::coroutine_handle<>> shared_waiters;
    std::deque<std::coroutine_handle<>> exclusive_waiters;
    std::mutex mutex;
public:
    SharedMutex() = default;
    ~SharedMutex() = default;

    class SharedLockAwaiter {
        SharedMutex& mutex;
    public:
        SharedLockAwaiter(SharedMutex& mutex) noexcept : mutex(mutex) {}
        bool await_ready() noexcept {
            std::lock_guard lock(mutex.mutex);
            if (!mutex.exclusive && mutex.exclusive_waiters.empty()) {
                ++mutex.shared_count;
                return true;
            }
            return false;
        }
        void await_suspend(std::coroutine_handle<> handle) noexcept {
            std::unique_lock lock(mutex.mutex);
            if (!mutex.exclusive && mutex.exclusive_waiters.empty()) {
                ++mutex.shared_count;
                lock.unlock();
                handle.resume();
                lock.lock();
            } else {
                mutex.shared_waiters.push_back(handle);
            }
        }
        void await_resume() noexcept {}
    };

    class ExclusiveLockAwaiter {
        SharedMutex& mutex;
    public:
        ExclusiveLockAwaiter(SharedMutex& mutex) noexcept : mutex(mutex) {}
        bool await_ready() noexcept {
            std::lock_guard lock(mutex.mutex);
            if (!mutex.exclusive && mutex.shared_count == 0) {
                mutex.exclusive = true;
                return true;
            }
            return false;
        }
        void await_suspend(std::coroutine_handle<> handle) noexcept {
            std::unique_lock lock(mutex.mutex);
            if (!mutex.exclusive && mutex.shared_count == 0) {
                mutex.exclusive = true;
                lock.unlock();
                handle.resume();
                lock.lock();
            } else {
                mutex.exclusive_waiters.push_back(handle);
            }
        }
        void await_resume() noexcept {}
    };
    SharedLockAwaiter lock_shared() noexcept {
        return SharedLockAwaiter(*this);
    }
    ExclusiveLockAwaiter lock() noexcept {
        return ExclusiveLockAwaiter(*this);
    }
    void unlock_shared() noexcept {
        std::unique_lock lock(mutex);
        if (--shared_count == 0 && !exclusive_waiters.empty()) {
            exclusive = true;
            auto handle = exclusive_waiters.front();
            exclusive_waiters.pop_front();
            lock.unlock();
            handle.resume();
            lock.lock();
        }
    }
    void unlock() noexcept {
        std::unique_lock lock(mutex);
        exclusive = false;
        if (!exclusive_waiters.empty()) {
            exclusive = true;
            auto handle = exclusive_waiters.front();
            exclusive_waiters.pop_front();
            lock.unlock();
            handle.resume();
            lock.lock();
        } else {
            std::deque<std::coroutine_handle<>> temp_waiters;
            std::swap(temp_waiters, shared_waiters);
            lock.unlock();
            while (!temp_waiters.empty()) {
                auto handle = temp_waiters.front();
                temp_waiters.pop_front();
                ++shared_count;
                handle.resume();
            }
            lock.lock();
        }
    }
};

template <typename MutexType = SharedMutex>
class SharedLock {
    MutexType& mutex;
#ifdef LCORE_DEBUG
    bool m_has_awaited = false;
#endif
public:
    SharedLock(MutexType& mutex) noexcept : mutex(mutex) {}
    ~SharedLock() noexcept {
#ifdef LCORE_DEBUG
        if (!m_has_awaited) {
            LCORE_ERROR("SharedLock was destroyed without awaiting");
        }
#endif
        mutex.unlock_shared();
    }
    auto operator co_await() & noexcept {
#ifdef LCORE_DEBUG
        m_has_awaited = true;
#endif
        return mutex.lock_shared();
    }
};

} // namespace async
LCORE_NAMESPACE_END
