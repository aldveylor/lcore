#include "lcore/async/executor.hpp"
#include <chrono>
#include <coroutine>
#include <exception>
#include <mutex>
#include <utility>

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::async;

// Scheduler

void Scheduler::DoAttachComponent(TypeIndex typeIndex, UniquePtr<Component> component)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto [it, inserted] = m_components.emplace(typeIndex, std::move(component));
    if (!inserted) {
        throw RuntimeError("Component of this type already exists in the scheduler.");
    }
}

Component& Scheduler::DoGetComponent(TypeIndex typeIndex)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(typeIndex);
    if (it == m_components.end()) {
        throw RuntimeError("Component of this type does not exist in the scheduler.");
    }
    return *it->second.Get();
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
        std::lock_guard<std::mutex> lock(m_mutex);
        // Immediately, not need for sperate lock
        for (auto it = m_taskstates.begin(); it != m_taskstates.end();) {
            auto& task = *it;
            if (task->done()) {
                it = m_taskstates.erase(it);
            } else {
                ++it;
            }
        }
        std::cout << "Current active tasks: " << m_taskstates.size() << std::endl;
    }
}

void Scheduler::DoSchedule(Ptr<StateBase>&& state) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_newtaskstates.push_back(std::move(state));
    }
    m_cv.notify_one();
}

Scheduler& Scheduler::GetThis() {
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
    if (&Scheduler::GetThis() != this) {
        throw RuntimeError("Run function must be called from the thread that owns the scheduler.");
    }
    m_running = true;
    auto checkContinue = [this]() {
        if (!m_running) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_taskstates.empty() || !m_newtaskstates.empty();
    };
    auto checkContinueWithNoLock = [this]() {
        if (!m_running) return false;
        return !m_taskstates.empty() || !m_newtaskstates.empty();
    };
    while (true) {
        if (!checkContinue()) break;
        Loop();
        for (auto& [typeIndex, component] : m_components) {
            component->Loop(*this);
        }
        if (!checkContinue()) break;


        Component::Duration waitDuration = Component::Duration::max();
        for (auto& [typeIndex, component] : m_components) {
            waitDuration = std::min(waitDuration, component->GetNextEventDuration());
        }
        bool needLoop = false;
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, waitDuration);
            needLoop = !m_newtaskstates.empty();
            needLoop |= !checkContinueWithNoLock(); // Lock is acquired, so we can safely check the condition
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
            if (needLoop) break;
        }
    }
    m_running = false;
}

void Scheduler::Stop() {
    m_running = false;
    m_cv.notify_all();
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

Lazy<void> async::Sleep(TimerComponent::Duration duration) {
    struct Awaiter {
        Scheduler& scheduler;
        TimerComponent::Duration duration;

        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> h) {
            auto time = std::chrono::steady_clock::now() + duration;
            scheduler.GetComponent<TimerComponent>().AddTimer(time, h);
        }

        void await_resume() const noexcept {}
    };
    co_await Awaiter{Scheduler::GetThis(), duration};
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
    co_await Awaiter{Scheduler::GetThis(), time};
}

Lazy<void> WrapTimeout(LazyTask<void> task, SharedPtr<bool> cancelled) {
    co_await std::suspend_always{};
    if (*cancelled) co_return;
    co_await std::move(task);
}

std::function<void()> async::SetTimeout(TimerComponent::Duration duration, LazyTask<void>&& task) {
    auto& scheduler = Scheduler::GetThis();
    auto time = std::chrono::steady_clock::now() + duration;
    auto& timerComponent = scheduler.GetComponent<TimerComponent>();
    auto cancelled = MakeShared<bool>(false);
    auto wrapTask = WrapTimeout(std::move(task), cancelled);
    auto handle = wrapTask.get_handle();
    scheduler.Schedule(std::move(wrapTask));
    timerComponent.AddTimer(time, handle);
    return [time, handle, &timerComponent, cancelled]() {
        if (!timerComponent.RemoveTimer(time, handle)) return; // Timer already triggered or cancelled
        *cancelled = true;
        timerComponent.AddTimer(std::chrono::steady_clock::now(), handle); // Resume immediately to cancel the task
    };
}

