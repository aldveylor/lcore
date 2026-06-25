/**
 * @file executor.hpp
 * @brief Task executor
 */
#pragma once
#include "base.hpp"
#include "lcore/traits.hpp"
#include "lcore/container/list.hpp"
#include "lcore/container/queue.hpp"
#include "sharedtask.hpp"
#include <coroutine>
#include <mutex>
#include <condition_variable>
#include <map>
#include <functional>
#include <utility>

LCORE_ASYNC_NAMESPACE_BEGIN

class Component;

class Scheduler {
    using StateBase = _detail::StateBase;

    std::map<TypeIndex, UniquePtr<Component>> m_components;
    List<Ptr<StateBase>> m_taskstates;
    List<Ptr<StateBase>> m_newtaskstates;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running = false;
    /// @brief Attach a component to the scheduler, the component will be initialized immediately
    /// @return true if the component is attached successfully, false if the component is already attached
    bool DoAttachComponent(TypeIndex, UniquePtr<Component>);
    RawPtr<Component> DoGetComponent(TypeIndex) const;
    UniquePtr<Component> DoDetachComponent(TypeIndex);
protected:
    /// @brief The main loop of the scheduler
    void Loop();
    /// @brief Schedule a task state to be executed in the next iteration of the scheduler loop
    void DoSchedule(Ptr<StateBase>&&);
    /// @brief Initialize the scheduler, called before the main loop starts
    /// Use Scheduler::GetThis() to get the current scheduler instance instead
    Scheduler() = default;
    ~Scheduler();

    static void DefaultExceptionHandler(std::exception_ptr e);
public:
    bool stopWhenIdle = true;                   /// If true, the scheduler will stop automaticly when there is no task to execute
    bool waitForConditionVariable = true;       /// If true, the scheduler will wait for the condition variable to be notified when there is no task to execute, otherwise it will busy wait
    std::function<void(std::exception_ptr)> exceptionHandler = DefaultExceptionHandler;  /// The exception handler, called when a task throws an exception, if not set, the exception will be ignored

    /// @brief Get the current scheduler instance, only awailable in current thread
    static Scheduler& GetInstance();
    /// @brief Attach a component to the scheduler, the component will be initialized immediately
    template <typename T, typename... Args>
    Scheduler& AttachComponent(Args&&... args) {
        this->DoAttachComponent(TypeIndex::Of<T>(), MakeUnique<T>(std::forward<Args>(args)...));
        return *this;
    }
    template <typename T, typename... Args>
    Scheduler& AttachComponentSafe(Args&&... args) {
        if (this->GetComponentSafe<T>()) return *this;  // avoid factory a component if it is already attached
        this->DoAttachComponent(TypeIndex::Of<T>(), MakeUnique<T>(std::forward<Args>(args)...));
        return *this;
    }
    /// @brief Get a reference to the component, or throw if the component is not attached
    template <typename T>
    T& GetComponent() {
        auto component = this->DoGetComponent(TypeIndex::Of<T>());
        if (!component) throw RuntimeError("Component not found");
        return static_cast<T&>(*component);
    }
    template <typename T>
    T* GetComponentSafe() {
        auto component = this->DoGetComponent(TypeIndex::Of<T>());
        return static_cast<T*>(component);
    }
    /// @brief Detach the component, the component will be finalized immediately, and returned as a unique pointer
    /// @return The detached component, or nullptr if the component is not attached
    template <typename T>
    UniquePtr<T> DetachComponent() {
        return this->DoDetachComponent(TypeIndex::Of<T>()).template Cast<T>();
    }

    /// @brief Run the scheduler loop
    void Run();
    /// @brief Stop the scheduler loop
    void Stop();
    /// @brief Notify the scheduler to wake up and handle events, called by components when an event is triggered
    void Notify() { m_cv.notify_one(); }
    /// @brief Wait until the scheduler is running
    void WaitRunning();

    /// @brief Schedule a task to be executed in the next iteration of the scheduler loop
    template <typename T>
    void Schedule(LazyTask<T>&& task) {
        auto raw_handle = std::move(task).release();
        DoSchedule(MakeShared<StateBase>(
            std::coroutine_handle<PromiseBase>::from_promise(raw_handle.promise())));
    }
    /// @brief Schedule a shared task to be executed in the next iteration of the scheduler loop
    template <typename T>
    void Schedule(SharedLazyTask<T>&& task) {
        DoSchedule(task.get_state());
    }
    /// @brief Schedule a function to be executed in the next iteration of the scheduler loop
    template <typename Func>
    requires InvokeAble<Func> && (!IsTask<ResultCallable<Func>>)
    void Schedule(Func&& func) {
        Schedule([](Func func) -> Lazy<void> {
            func();
            co_return;
        }(std::move(func)));
    }
};

/**
 * @brief A module that can be attached to the scheduler to handle events, such as timers, I/O events, etc.
 * The component will be initialized when attached to the scheduler, and finalized when detached from the scheduler
 */
class Component: public AbstractClass {
public:
    using Duration = std::chrono::steady_clock::duration;

    /// @brief Initialize the component, called when the component is attached to the scheduler
    virtual void DoInitialize(Scheduler&) {}

    /// @brief Finalize the component, called when the component is detached from the scheduler
    virtual void DoFinalize(Scheduler&) {}

    /// @brief Get the duration until the next event, if the component has an timeout event to handle, otherwise return `Duration::max()`
    virtual Duration GetNextEventDuration() const { return Duration::max(); }
    /// @brief Handle the event, called when the condition variable is notified or the timeout expires
    /// return true if an event is handled
    /// If any event is handled, the scheduler will enter loop
    virtual bool HandleEvent(Scheduler&) { return false; }

    /// @brief Loop function called in each iteration of the scheduler loop
    virtual void Loop(Scheduler&) {}
};

class TimerComponent: public Component {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
private:
    struct Timer {
        TimePoint time;
        std::coroutine_handle<> handle;

        bool operator>(const Timer& other) const { return time > other.time; }
        void resume() && { handle.resume(); }
    };
    IterablePriorityQueue<Timer, std::vector<Timer>, std::greater<>> m_timers;
    mutable std::mutex m_mutex;

    Scheduler* m_scheduler = nullptr;
public:
    void DoInitialize(Scheduler&) override;
    void DoFinalize(Scheduler&) override;
    void AddTimer(TimePoint time, std::coroutine_handle<> handle);
    /// @brief Remove a timer, return true if the timer is removed, false if the timer is not found or already expired
    bool RemoveTimer(TimePoint time, std::coroutine_handle<> handle);
    Duration GetNextEventDuration() const override;
    bool HandleEvent(Scheduler&) override;
};

/// @brief Sleep for a duration, the task will be resumed after the duration expires
/// @note Called only in executor context, otherwise the behavior is undefined
Lazy<void> Sleep(TimerComponent::Duration duration);
/// @brief Sleep until a time point, the task will be resumed after the time point is reached
/// @note Called only in executor context, otherwise the behavior is undefined
Lazy<void> SleepUntil(TimerComponent::TimePoint time);
/// @brief Set a timeout for the task, the task will be executed after the timeout expires
/// @return A function that can be called to cancel the timeout, if the timeout is not expired yet
/// @note Called only in executor context, otherwise the behavior is undefined
std::function<void()> SetTimeout(TimerComponent::Duration timeout, LazyTask<void>&& task);
/// @brief Set a timeout for the task, the task will be executed after the timeout expires
/// @param func The function to be called when the timeout expires
/// @return A function that can be called to cancel the timeout, if the timeout is not expired yet
/// @note Called only in executor context, otherwise the behavior is undefined
template <typename Func>
requires InvokeAble<Func>
auto SetTimeout(TimerComponent::Duration timeout, Func&& func) {
    return SetTimeout(timeout, [](Func func) -> Lazy<void> {
        func();
        co_return;
    }(std::move(func)));
}
/// @brief Set an interval for the task, the task will be executed repeatedly with the interval until the returned cancel function is called
/// @return A function that can be called to cancel the interval, if the interval is not canceled yet
/// @note Called only in executor context, otherwise the behavior is undefined
template <typename Func>
requires InvokeAble<Func>
auto SetInterval(TimerComponent::Duration interval, Func&& func) {
    using FuncType = std::decay_t<Func>;
    struct State {
        std::function<void()> cancel;
        std::function<void()> set_next;
        FuncType func;
        State(Func func): func(std::move(func)) {}
    };
    auto state = MakeShared<State>(std::forward<Func>(func));
    state->set_next = [state, interval]() {
        state->cancel = SetTimeout(interval, [state]() {
            state->func();
            state->set_next();
        });
    };
    state->set_next();
    return [state]() { state->cancel(); };
}

LCORE_ASYNC_NAMESPACE_END
