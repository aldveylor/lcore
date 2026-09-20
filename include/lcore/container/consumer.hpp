#pragma once
#include "../base.hpp"
#include <deque>
#include <mutex>
#include <condition_variable>

LCORE_NAMESPACE_BEGIN

/// @brief A consumer queue is a thread-safe queue that allows multiple consumers to retrieve items from it.
template <typename T,
          typename Container = std::deque<T>>
class ConsumerQueue{
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::atomic<int> m_resetCount;
    Container m_container;
public:
    /// @brief Push an item into the queue.
    /// @param item The item to be pushed into the queue.
    void push(T&& item){
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_container.push_back(std::move(item));
        }
        m_cv.notify_one();
    }
    /// @brief Reset the queue, clearing all items and notifying all waiting consumers.
    void reset(){
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_container.clear();
            m_resetCount++;
        }
        m_cv.notify_all();
    }
    /// @brief Pop an item from the queue. If the queue is empty, it will wait until an item is available or the queue is reset.
    std::optional<T> pop(){
        std::unique_lock<std::mutex> lock(m_mtx);
        int currentResetCount = m_resetCount.load();
        m_cv.wait(lock, [this, currentResetCount]{
            return !m_container.empty() || m_resetCount.load() != currentResetCount;
        });
        if(m_resetCount.load() != currentResetCount){
            return std::nullopt; // Queue was reset
        }
        T item = std::move(m_container.front());
        m_container.pop_front();
        return item;
    }
};

LCORE_NAMESPACE_END
