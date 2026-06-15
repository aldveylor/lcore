#pragma once
#include "base.hpp"
#include "lcore/exception.hpp"
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>

LCORE_NAMESPACE_BEGIN

template <typename T>
class Promise;

template <typename T>
class Future {
    Promise<T>& m_promise;
public:
    using ValueType = T;
    using Args = typename Promise<T>::Args;
    using CallbackType = typename Promise<T>::CallbackType;

    Future(Promise<T>& promise) : m_promise(promise) {}
    bool Then(CallbackType&& callback) {
        return m_promise.Then(std::forward<CallbackType>(callback));
    }
    bool Wait() { return m_promise.Wait(); }
    T Get() requires(CopyConstructible<T> && MoveConstructible<T>) {
        if (m_promise.Done()) throw RuntimeError("Promise already fulfilled");
        T result;
        m_promise.Then([&result](Args value) {
            result = value;
        });
        Wait();
        return result;
    }
};

template <typename T>
class Promise {
public:
    using FutureType = Future<T>;
    using Args = std::conditional_t<
        std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(void*),
        T,
        const T&
    >;
    using CallbackType = std::function<void(Args)>;
private:
    bool m_done;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::vector<CallbackType> m_callbacks;
public:
    Promise() : m_done(false) {}

    void Complete(Args value) {
        std::vector<CallbackType> _callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_done) throw RuntimeError("Promise already fulfilled");
            m_done = true;
            m_callbacks.swap(_callbacks);
        }
        for (const auto& callback : _callbacks) {
            callback(value);
        }
        m_cond.notify_all();
    }

    bool Then(CallbackType callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_done) return false;
        m_callbacks.push_back(callback);
        return true;
    }

    bool Wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_done) return false;
        m_cond.wait(lock, [this] { return m_done; });
        return true;
    }

    Future<T> GetFuture() {
        return Future<T>(*this);
    }
};

template <>
class Promise<void> {
public:
    using FutureType = Future<void>;
    using Args = void;
    using CallbackType = std::function<void()>;
private:
    bool m_done;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::vector<CallbackType> m_callbacks;
public:
    inline Promise() : m_done(false) {}

    inline void SetValue();
    inline bool Then(CallbackType callback);
    inline bool Wait();
    inline bool Done();

    inline Future<void> GetFuture();
};

template <>
class Future<void> {
    Promise<void>& m_promise;
public:
    using ValueType = void;    
    using CallbackType = typename Promise<void>::CallbackType;
    using Args = typename Promise<void>::Args;

    inline Future(Promise<void>& promise) : m_promise(promise) {}
    inline bool Then(std::function<void()> callback);
    inline bool Wait();
};

inline void Promise<void>::SetValue() {
    std::vector<CallbackType> _callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_done) throw RuntimeError("Promise already fulfilled");
        m_done = true;
        m_callbacks.swap(_callbacks);
    }
    for (const auto& callback : _callbacks) {
        callback();
    }
    m_cond.notify_all();
}

inline bool Promise<void>::Then(CallbackType callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_done) return false;
    m_callbacks.push_back(callback);
    return true;
}

inline bool Promise<void>::Wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_done) return false;
    m_cond.wait(lock, [this] { return m_done; });
    return true;
}

inline bool Promise<void>::Done() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_done;
}

inline Future<void> Promise<void>::GetFuture() {
    return Future<void>(*this);
}

inline bool Future<void>::Then(std::function<void()> callback) {
    return m_promise.Then(callback);
}
inline bool Future<void>::Wait() {
    return m_promise.Wait();
}

LCORE_NAMESPACE_END
