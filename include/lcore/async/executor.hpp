/**
 * @file executor.hpp
 * @brief Task executor
 */
// #include "base.hpp"
// #include "traits.hpp"
// #include "lcore/container.hpp"
// #include "task.hpp"
// #include <optional>
// #include <thread>
// #include <mutex>
// #include <condition_variable>

// LCORE_ASYNC_NAMESPACE_BEGIN

// template <template <typename> typename TaskType = DefaultTaskWrapper>
// class Executor: public AbstractClass {
// protected:
//     virtual void DoSchedule(TaskType<void>&& task) = 0;
// public:
//     inline void Schedule(TaskType<void>&& task) {
//         DoSchedule(std::move(task));
//     }
//     template <typename T>
//     inline void Schedule(TaskType<T>&& task) {
//         DoSchedule([task = std::move(task)]() mutable -> TaskType<void> {
//             co_await std::move(task);
//         }());
//     }
//     virtual void Run() = 0;
//     virtual void Stop() = 0;
// };

// /// @brief A simple executor that runs tasks in current thread.
// /// Not thread-safe, should be used in single thread.
// /// When all tasks are done, it will stop automatically.
// template <template <typename> typename TaskWrapper = DefaultTaskWrapper>
// class DefaultExecutor: public Executor<TaskWrapper> {
//     using TaskType = TaskWrapper<void>;
// private:
//     List<TaskType> m_coroutines;
//     bool m_stopped = true;
// protected:
//     void DoSchedule(TaskType&& task) override {
//         m_coroutines.emplace_back(std::move(task));
//     }
// public:
//     bool sleepIfNoReadyTasks = true;
//     void Run() override {
//         m_stopped = false;
//         while (!m_stopped && !m_coroutines.empty()){
//             bool allSuspended = true;
//             for (auto iter = m_coroutines.begin(); iter != m_coroutines.end();){
//                 auto &coroutine = *iter;
//                 if (coroutine.done()){
//                     iter = m_coroutines.erase(iter);
//                     allSuspended = false;
//                 }else{
//                     iter++;
//                 }
//             }
//             if (sleepIfNoReadyTasks && allSuspended) {
//                 std::this_thread::yield();
//             }
//         }
//         m_stopped = true;
//     }
//     void Stop() override {
//         m_stopped = true;
//     }
// };

// /// @brief A thread-safe executor that runs tasks in current thread.
// /// When all tasks are done, it will stop automatically.
// template <template <typename> typename TaskWrapper = DefaultTaskWrapper>
// class ThreadSafeExecutor: public Executor<TaskWrapper> {
//     using TaskType = TaskWrapper<void>;
// private:
//     List<TaskType> m_coroutines;
//     std::mutex m_mutex;
//     bool m_stopped = true;
// protected:
//     void DoSchedule(TaskType&& task) override {
//         std::lock_guard<std::mutex> lock(m_mutex);
//         m_coroutines.push_back(std::move(task));
//     }
// public:
//     bool sleepIfNoReadyTasks = true;
//     void Run() override {
//         auto _list_begin = [this]() {
//             std::lock_guard<std::mutex> lock(m_mutex);
//             return m_coroutines.begin();
//         };
//         auto _list_end = [this]() {
//             std::lock_guard<std::mutex> lock(m_mutex);
//             return m_coroutines.end();
//         };
//         auto _list_erase = [this](List<TaskType>::iterator iter) {
//             std::lock_guard<std::mutex> lock(m_mutex);
//             return m_coroutines.erase(iter);
//         };
//         auto _list_empty = [this]() {
//             std::lock_guard<std::mutex> lock(m_mutex);
//             return m_coroutines.empty();
//         };
//         m_stopped = false;
//         while (!m_stopped && !_list_empty()){
//             bool allSuspended = true;
//             for (auto iter = _list_begin(); iter != _list_end();){
//                 auto &coroutine = *iter;
//                 if (coroutine.done()){
//                     iter = _list_erase(iter);
//                 }else{
//                     iter++;
//                 }
//             }
//             if (sleepIfNoReadyTasks && allSuspended) {
//                 std::this_thread::yield();
//             }
//         }
//         m_stopped = true;
//     }
//     void Stop() override {
//         m_stopped = true;
//     }
// };

// /// @brief A threaded executor that runs tasks in another thread.
// /// Will not stop automatically, need to call Stop() to stop the worker thread.
// template <template <typename> typename TaskWrapper = DefaultTaskWrapper>
// class ThreadedExecutor: public Executor<TaskWrapper> {
//     using TaskType = TaskWrapper<void>;
// private:
//     List<TaskType> m_coroutines;
//     std::mutex m_coroutinesMutex;
//     std::condition_variable m_coroutinesCV;

//     std::optional<std::thread> m_worker;
//     bool m_stopped = true;
// protected:
//     void ThreadFunc() {
//         auto _list_begin = [this]() {
//             std::lock_guard<std::mutex> lock(m_coroutinesMutex);
//             return m_coroutines.begin();
//         };
//         auto _list_end = [this]() {
//             std::lock_guard<std::mutex> lock(m_coroutinesMutex);
//             return m_coroutines.end();
//         };
//         auto _list_erase = [this](List<TaskType>::iterator iter) {
//             std::lock_guard<std::mutex> lock(m_coroutinesMutex);
//             return m_coroutines.erase(iter);
//         };
//         auto _list_empty = [this]() {
//             std::lock_guard<std::mutex> lock(m_coroutinesMutex);
//             return m_coroutines.empty();
//         };
//         m_stopped = false;
//         while (!m_stopped){
//             if (_list_empty()){
//                 std::unique_lock<std::mutex> lock(m_coroutinesMutex);
//                 m_coroutinesCV.wait(lock, [this, &_list_empty](){ return m_stopped || !_list_empty(); });
//                 if (m_stopped) break;
//             }
//             bool allSuspended = true;
//             for (auto iter = _list_begin(); iter != _list_end();){
//                 auto &coroutine = *iter;
//                 if (coroutine.done()){
//                     iter = _list_erase(iter);
//                     allSuspended = false;
//                 }else{
//                     iter++;
//                 }
//             }
//             if (sleepIfNoReadyTasks && allSuspended) {
//                 std::this_thread::yield();
//             }
//         }
//         m_stopped = true;
//     }
//     void DoSchedule(TaskType&& task) override {
//         {
//             std::lock_guard<std::mutex> lock(m_coroutinesMutex);
//             m_coroutines.push_back(std::move(task));
//         }
//         m_coroutinesCV.notify_one();
//     }
// public:
//     bool sleepIfNoReadyTasks = true;
//     /// Not blocked
//     void Run() override {
//         m_worker = std::thread(&ThreadedExecutor::ThreadFunc, this);
//     }
//     void Stop() override {
//         m_stopped = true;
//         m_coroutinesCV.notify_all();
//         if (m_worker.has_value()) {
//             if (m_worker->joinable()) {
//                 m_worker->join();
//             }
//             m_worker.reset();
//         }
//     }
// };

// LCORE_ASYNC_NAMESPACE_END
