/**
 * @file executor.hpp
 * @author liyanes@outlook.com
 * @brief Task executor
 * @version 0.1
 * @date 2024-08-31
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "base.hpp"
#include "lcore/container.hpp"
#include "task.hpp"
#include <optional>
#include <thread>
#include <mutex>
#include <condition_variable>

LCORE_ASYNC_NAMESPACE_BEGIN

class CoroutineHandleLifeTimeManager {
    std::coroutine_handle<> handle;
    bool owned = false;
public:
    CoroutineHandleLifeTimeManager() = default;
    explicit CoroutineHandleLifeTimeManager(std::coroutine_handle<> handle): handle(handle), owned(true) {}
    explicit CoroutineHandleLifeTimeManager(std::coroutine_handle<> handle, bool owned): handle(handle), owned(owned) {}
    CoroutineHandleLifeTimeManager(const CoroutineHandleLifeTimeManager&) = delete;
    CoroutineHandleLifeTimeManager(CoroutineHandleLifeTimeManager&& other) noexcept: handle(other.handle), owned(other.owned) {
        other.handle = nullptr;
        other.owned = false;
    }
    ~CoroutineHandleLifeTimeManager() {
        if (owned && handle){
            handle.destroy();
        }
    }
    CoroutineHandleLifeTimeManager& operator=(const CoroutineHandleLifeTimeManager&) = delete;
    CoroutineHandleLifeTimeManager& operator=(CoroutineHandleLifeTimeManager&& other) noexcept {
        if (this != &other){
            if (owned && handle){
                handle.destroy();
            }
            handle = other.handle;
            owned = other.owned;
            other.handle = nullptr;
            other.owned = false;
        }
        return *this;
    }
    std::coroutine_handle<> GetHandle() const {
        return handle;
    }
    bool done() const {
        return !handle || handle.done();
    }
    void resume() {
        if (handle){
            handle.resume();
        }
    }
    void destroy() {
        if (handle){
            handle.destroy();
            handle = nullptr;
            owned = false;
        }
    }
};

class Executor: public AbstractClass {
protected:
    virtual void DoSchedule(CoroutineHandleLifeTimeManager task) = 0;
public:

    template <IsTask TaskType>
    void Schedule(TaskType&& task) {
        DoSchedule(CoroutineHandleLifeTimeManager(task.get_handle(), false));
    }
    void Schedule(std::coroutine_handle<> handle) {
        DoSchedule(CoroutineHandleLifeTimeManager(handle, false));
    }
    virtual void Run() = 0;
    virtual void Stop() = 0;
};

/// @brief A simple executor that runs tasks in current thread.
/// Not thread-safe, should be used in single thread.
/// When all tasks are done, it will stop automatically.
class DefaultExecutor: public Executor {
private:
    List<CoroutineHandleLifeTimeManager> m_coroutines;
    bool m_stopped = true;
protected:
    void DoSchedule(CoroutineHandleLifeTimeManager task) override;
public:
    bool sleepIfNoReadyTasks = true;
    void Run() override;
    void Stop() override;
};

/// @brief A thread-safe executor that runs tasks in current thread.
/// When all tasks are done, it will stop automatically.
class ThreadSafeExecutor: public Executor {
private:
    List<CoroutineHandleLifeTimeManager> m_coroutines;
    std::mutex m_mutex;
    bool m_stopped = true;
protected:
    void DoSchedule(CoroutineHandleLifeTimeManager task) override;
public:
    bool sleepIfNoReadyTasks = true;
    void Run() override;
    void Stop() override;
};

/// @brief A threaded executor that runs tasks in another thread.
/// Will not stop automatically, need to call Stop() to stop the worker thread.
class ThreadedExecutor: public Executor {
private:
    List<CoroutineHandleLifeTimeManager> m_coroutines;
    std::mutex m_coroutinesMutex;
    std::condition_variable m_coroutinesCV;

    std::optional<std::thread> m_worker;
    bool m_stopped = true;
protected:
    void ThreadFunc();
    void DoSchedule(CoroutineHandleLifeTimeManager task) override;
public:
    bool sleepIfNoReadyTasks = true;
    /// Not blocked
    void Run() override;
    void Stop() override;
};

LCORE_ASYNC_NAMESPACE_END
