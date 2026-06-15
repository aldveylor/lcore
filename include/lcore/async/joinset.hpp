#pragma once
#include "task.hpp"
#include "lcore/container/list.hpp"
#include "lcore/assert.hpp"
#include "lcore/threadsafe.hpp"
#include <deque>

LCORE_ASYNC_NAMESPACE_BEGIN

template <typename T>
struct JoinResult {
    std::optional<T> value;
    std::exception_ptr exception;
};

/// @brief A join set that can hold multiple tasks and wait for all of them to complete
template <typename T>
class JoinSet {
    List<Lazy<T>> tasks;
    mutable std::mutex mtx;
    std::deque<JoinResult<T>> completed;
    std::deque<std::coroutine_handle<>> waiters;
public:
    JoinSet() = default;
    ~JoinSet() {
        if (!tasks.empty()) {
            // Should prompt a warning here
            LCORE_LOG("[Warning] JoinSet destroyed with unfinished tasks");
        }
    }
    void spawn(Lazy<T>&& task) {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push_back(std::move(task));
    }
    auto next() {
        struct Awaiter {
            JoinSet* self;
            std::optional<JoinResult<T>> result;

            bool await_ready() {
                return self->try_pop_result(result) || self->empty();
            }

            void await_suspend(std::coroutine_handle<> h) {
                if (!result.has_value()) {
                    std::lock_guard<std::mutex> lock(self->mtx);
                    self->waiters.push_back(h);
                    self->poll();
                } else {
                    h.resume();
                }
            }

            std::optional<JoinResult<T>> await_resume() {
                return std::move(result);
            }
        };
        return Awaiter{this, std::nullopt};
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return tasks.empty() && completed.empty();
    }

private:
    void poll() {
        for (auto it = tasks.begin(); it != tasks.end();) {
            if (!it->done()) it->resume();
            if (it->done()) {
                JoinResult<T> jr;
                try {
                    jr.value = it->consume_value();
                } catch (...) {
                    jr.exception = std::current_exception();
                }
                completed.push_back(std::move(jr));
                it = tasks.erase(it);
            } else {
                ++it;
            }
        }
        while (!completed.empty() && !waiters.empty()) {
            auto w = waiters.front();
            waiters.pop_front();
            w.resume();
        }
    }
    
    bool try_pop_result(std::optional<JoinResult<T>>& out) {
        if (!completed.empty()) {
            out = std::move(completed.front());
            completed.pop_front();
            return true;
        }
        return false;
    }
};

LCORE_ASYNC_NAMESPACE_END
