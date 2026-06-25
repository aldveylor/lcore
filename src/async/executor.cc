#include "lcore/async/executor.hpp"
#include "lcore/logger.hpp"
#include <chrono>
#include <coroutine>
#include <exception>
#include <mutex>
#include <utility>

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::async;

USE_LOGGER("async.executor");

// Scheduler

Scheduler::~Scheduler() {
    LCORE_ASSERT(!m_running, "Scheduler must be stopped before destruction.");
    std::unique_lock<std::mutex> lock(m_mutex);
    for (auto& [typeIndex, component] : m_components) {
        lock.unlock();
        component->DoFinalize(*this);
        lock.lock();
    }
}

bool Scheduler::DoAttachComponent(TypeIndex typeIndex, UniquePtr<Component> component)
{
    if (m_running) {
        throw RuntimeError("Cannot attach component while scheduler is running.");
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    auto [it, inserted] = m_components.emplace(typeIndex, std::move(component));
    if (inserted) {
        lock.unlock();
        try {
            it->second->DoInitialize(*this);
        }
        catch (...)
        {
            m_components.erase(it);
            std::rethrow_exception(std::current_exception());
        }
        lock.lock();
        LOG_DEBUG(std::format("Component of type {} attached to scheduler.", typeIndex.name()));
    }
    return inserted;
}

RawPtr<Component> Scheduler::DoGetComponent(TypeIndex typeIndex) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(typeIndex);
    if (it == m_components.end()) {
        return nullptr;
    }
    return it->second.Get();
}

UniquePtr<Component> Scheduler::DoDetachComponent(TypeIndex typeIndex)
{
    if (m_running) {
        throw RuntimeError("Cannot detach component while scheduler is running.");
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_components.find(typeIndex);
    if (it == m_components.end()) {
        return nullptr;
    }
    lock.unlock();
    it->second->DoFinalize(*this);
    lock.lock();
    UniquePtr<Component> component = std::move(it->second);
    m_components.erase(it);
    return component;
}

void Scheduler::Loop() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_newtaskstates.empty()) {
            List<Ptr<StateBase>> newTasks;
            newTasks.swap(m_newtaskstates);
            lock.unlock();
            for (auto& task : newTasks) {
                task->resume();
                lock.lock();
                m_taskstates.push_back(std::move(task));
                lock.unlock();
            }
        }
    }
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Immediately, not need for sperate lock
        for (auto it = m_taskstates.begin(); it != m_taskstates.end();) {
            auto task = *it;
            if (task->done()) {
                it = m_taskstates.erase(it);
                if (this->exceptionHandler && task->has_exception()) {
                    lock.unlock();
                    this->exceptionHandler(task->get_exception());
                    lock.lock();
                }
            } else {
                ++it;
            }
        }
    }
}

void Scheduler::DoSchedule(Ptr<StateBase>&& state) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_newtaskstates.push_back(std::move(state));
    }
    m_cv.notify_one();
}

void Scheduler::DefaultExceptionHandler(std::exception_ptr eptr) {
    try {
        if (eptr) {
            std::rethrow_exception(eptr);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Unhandled exception in scheduler: {}", e.what()));
    } catch (...) {
        LOG_ERROR("Unhandled unknown exception in scheduler");
#ifdef LCORE_DEBUG
        
#endif
    }
}

#include <pthread.h>

Scheduler& Scheduler::GetInstance() {
    static thread_local Scheduler scheduler;
    static thread_local bool initialized = false;
    if (!initialized) {
        scheduler
            .AttachComponent<TimerComponent>();
        initialized = true;
    }
    return scheduler;
}

void Scheduler::Run() {
    if (&Scheduler::GetInstance() != this) {
        throw RuntimeError("Run function must be called from the thread that owns the scheduler.");
    }
    auto checkContinue = [this]() {
        if (!m_running) return false;
        if (!stopWhenIdle) return true;
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_taskstates.empty() || !m_newtaskstates.empty();
    };
    auto checkEventWithNoLock = [this]() {
        return !m_running ||  // Stop event
               !m_newtaskstates.empty(); // New task event
    };
    auto waitEventWithConditionVariable = [&, this](Component::Duration waitDuration) {
        bool needLoop = false;
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (checkEventWithNoLock()) {
                needLoop = true;
                break; // Exit the loop immediately if we have new tasks or events to handle
            }
            m_cv.wait_for(lock, waitDuration);
            needLoop = !checkEventWithNoLock(); // Lock is acquired, so we can safely check the condition
            lock.unlock();
            try {
                for (auto& [typeIndex, component] : m_components) {
                    needLoop |= component->HandleEvent(*this);
                }
            } catch (...) {
                std::rethrow_exception(std::current_exception());
            }
            if (needLoop) break; // Exit the loop if we have new tasks or events to handle
        }
        return needLoop;
    };
    auto checkContinueWithNoLock = [this]() {
        if (!m_running) return false;
        if (!stopWhenIdle) return true;
        return !m_taskstates.empty() || !m_newtaskstates.empty();
    };
    auto waitEvent = [&, this]() {
        bool needLoop = false;
        while (!needLoop) { // Busy-waiting loop
            // Handle events without waiting for condition variable, suitable for busy-waiting scenarios
            std::unique_lock<std::mutex> lock(m_mutex);
            needLoop = !m_newtaskstates.empty();
            needLoop |= !checkContinueWithNoLock();
            lock.unlock();
            try {
                for (auto& [typeIndex, component] : m_components) {
                    if (component->HandleEvent(*this)) {
                        needLoop = true;
                    }
                }
            } catch (...) {
                lock.lock();
                std::rethrow_exception(std::current_exception());
            }
            // Exit busy-waiting loop if we have new tasks or events to handle
        }
        return needLoop;
    };
    m_running = true;
    LOG_DEBUG(std::format("[I: {:p}] Scheduler started.", static_cast<void*>(this)));
    m_cv.notify_all(); // Notify any waiting threads that the scheduler has started
    while (true) {
        if (!checkContinue()) break;
        Loop();
        for (auto& [typeIndex, component] : m_components) {
            component->Loop(*this);
        }
        if (!checkContinue()) break;

        if (this->waitForConditionVariable) {
            Component::Duration waitDuration = Component::Duration::max();
            for (auto& [typeIndex, component] : m_components) {
                waitDuration = std::min(waitDuration, component->GetNextEventDuration());
            }
            waitEventWithConditionVariable(waitDuration);
        } else {
            waitEvent();
        }
    }
    LOG_DEBUG(std::format("[I: {:p}] Scheduler stopped.", static_cast<void*>(this)));
    m_running = false;
}

void Scheduler::Stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return; // Scheduler is already stopped
    }
    m_cv.notify_all();
}

void Scheduler::WaitRunning() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]() { return m_running.load(); });
}

// Time Component

void TimerComponent::AddTimer(TimePoint time, std::coroutine_handle<> handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timers.push(Timer{time, handle});
    // Scheduler::GetThis().Notify();
}

bool TimerComponent::RemoveTimer(TimePoint time, std::coroutine_handle<> handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& raw = m_timers.data();
    for (auto it = raw.begin(); it != raw.end();) {
        if (it->time == time && it->handle == handle) {
            it = raw.erase(it);
            return true;
        } else {
            ++it;
        }
    }
    return false;
}

Component::Duration TimerComponent::GetNextEventDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_timers.empty()) {
        return Duration::max();
    }
    auto now = std::chrono::steady_clock::now();
    auto nextTime = m_timers.data().front().time;
    if (nextTime <= now) {
        return Duration::zero();
    }
    return nextTime - now;
}

bool TimerComponent::HandleEvent(Scheduler&) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    bool handled = false;
    while (!m_timers.empty() && m_timers.top().time <= now) {
        auto timer = m_timers.top();
        m_timers.pop();
        lock.unlock();
        timer.handle.resume();
        lock.lock();
        handled = true;
    }
    return handled;
}

Lazy<void> async::SleepUntil(TimerComponent::TimePoint time) {
    struct Awaiter {
        Scheduler& scheduler;
        TimerComponent::TimePoint time;

        bool await_ready() const noexcept {
            return std::chrono::steady_clock::now() >= time;
        }

        void await_suspend(std::coroutine_handle<> h) {
            scheduler.GetComponent<TimerComponent>().AddTimer(time, h);
        }

        void await_resume() const noexcept {}
    };
    co_await Awaiter{Scheduler::GetInstance(), time};
}

Lazy<void> async::Sleep(TimerComponent::Duration duration) {
    return SleepUntil(std::chrono::steady_clock::now() + duration);
}

std::function<void()> async::SetTimeout(TimerComponent::Duration duration, LazyTask<void>&& task) {
    auto& scheduler = Scheduler::GetInstance();
    auto time = std::chrono::steady_clock::now() + duration;
    auto& timerComponent = scheduler.GetComponent<TimerComponent>();
    auto cancelled = MakeShared<bool>(false);
    auto wrapTask = [](LazyTask<void> task, SharedPtr<bool> cancelled) -> Lazy<void> {
        co_await std::suspend_always{};
        if (*cancelled) co_return;
        co_await std::move(task);
    }(std::move(task), cancelled);
    auto handle = wrapTask.get_handle();
    scheduler.Schedule(std::move(wrapTask));
    timerComponent.AddTimer(time, handle);
    return [time, handle, &timerComponent, cancelled]() {
        if (!timerComponent.RemoveTimer(time, handle)) return; // Timer already triggered or cancelled
        *cancelled = true;
        timerComponent.AddTimer(std::chrono::steady_clock::now(), handle); // Resume immediately to cancel the task
    };
}

