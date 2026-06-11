#include "lcore/async/executor.hpp"
#include "lcore/container/list.hpp"

using namespace LCORE_NAMESPACE_NAME;
using namespace LCORE_NAMESPACE_NAME::async;

void DefaultExecutor::DoSchedule(CoroutineHandleLifeTimeManager task) {
    m_coroutines.push_back(task);
}

void DefaultExecutor::Run()  {
    m_stopped = false;
    while (!m_stopped && !m_coroutines.empty()){
        for (auto iter = m_coroutines.begin(); iter != m_coroutines.end();){
            auto &coroutine = *iter;
            if (coroutine.done()){
                iter = m_coroutines.erase(iter);
            }else{
                coroutine.resume();
                iter++;
            }
        }
        if (sleepIfNoReadyTasks) {
            std::this_thread::yield();
        }
    }
    m_stopped = true;
}

void DefaultExecutor::Stop() {
    m_stopped = true;
}

void ThreadSafeExecutor::DoSchedule(CoroutineHandleLifeTimeManager task) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_coroutines.push_back(task);
}

void ThreadSafeExecutor::Run()  {
    auto _list_begin = [this]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coroutines.begin();
    };
    auto _list_end = [this]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coroutines.end();
    };
    auto _list_erase = [this](List<CoroutineHandleLifeTimeManager>::iterator iter) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coroutines.erase(iter);
    };
    auto _list_empty = [this]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coroutines.empty();
    };
    m_stopped = false;
    while (!m_stopped && !_list_empty()){
        for (auto iter = _list_begin(); iter != _list_end();){
            auto &coroutine = *iter;
            if (coroutine.done()){
                iter = _list_erase(iter);
            }else{
                coroutine.resume();
                iter++;
            }
        }
        if (sleepIfNoReadyTasks) {
            std::this_thread::yield();
        }
    }
    m_stopped = true;
}

void ThreadSafeExecutor::Stop() {
    m_stopped = true;
}


void ThreadedExecutor::ThreadFunc() {
    auto _list_begin = [this]() {
        std::lock_guard<std::mutex> lock(m_coroutinesMutex);
        return m_coroutines.begin();
    };
    auto _list_end = [this]() {
        std::lock_guard<std::mutex> lock(m_coroutinesMutex);
        return m_coroutines.end();
    };
    auto _list_erase = [this](List<CoroutineHandleLifeTimeManager>::iterator iter) {
        std::lock_guard<std::mutex> lock(m_coroutinesMutex);
        return m_coroutines.erase(iter);
    };
    auto _list_empty = [this]() {
        std::lock_guard<std::mutex> lock(m_coroutinesMutex);
        return m_coroutines.empty();
    };
    m_stopped = false;
    while (!m_stopped){
        if (_list_empty()){
            std::unique_lock<std::mutex> lock(m_coroutinesMutex);
            m_coroutinesCV.wait(lock, [this, &_list_empty](){ return m_stopped || !_list_empty(); });
            if (m_stopped) break;
        }
        for (auto iter = _list_begin(); iter != _list_end();){
            auto &coroutine = *iter;
            if (coroutine.done()){
                iter = _list_erase(iter);
            }else{
                coroutine.resume();
                iter++;
            }
        }
        if (sleepIfNoReadyTasks) {
            std::this_thread::yield();
        }
    }
    m_stopped = true;
}

void ThreadedExecutor::DoSchedule(CoroutineHandleLifeTimeManager task) {
    {
        std::lock_guard<std::mutex> lock(m_coroutinesMutex);
        m_coroutines.push_back(task);
    }
    m_coroutinesCV.notify_one();
}


