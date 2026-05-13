#pragma once
#include "base.hpp"
#include "traits.hpp"
#include "exception.hpp"
#include "rawptr.hpp"
#include <atomic>
#include <memory>

LCORE_NAMESPACE_BEGIN

// Forward declarations
template <typename T>
class RawPtr;

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

// Exceptions

class BadWeakPtr: public Exception {
public:
    using Exception::Exception; // Inherit constructors from Exception
    inline const char* what() const noexcept override {
        return "Bad weak pointer: The weak pointer is expired or not initialized.";
    }
};

namespace detail {

// Control block for SharedPtr & WeakPtr
template <template <typename> typename AtomicType = std::atomic>
class ControlBlockBase {
public:
    AtomicType<size_t> shared_count = 1; // Start with 1 for the initial shared_ptr
    AtomicType<size_t> weak_count = 0;

    /// @brief Destroy the object (Do not deallocate the memory!!!)
    virtual void Destory() = 0;
    /// @brief Deallocate the memory of this control block
    virtual void Deallocate() = 0;
    virtual ~ControlBlockBase() = default;

    void Ref() noexcept { ++shared_count; }
    bool Unref() {
        if (--shared_count == 0) {
            /**
             * Why weak_count is cached here?
             * When Destory() is called, the target object may own weak references to this control block.
             * WeakUnref() will be called and weak_count will be decremented.
             * if weak_count reaches 0, this control block will be deallocated by WeakUnref().
             * When Destory() is returned, this control block is deallocated and the weak_count is no longer valid.
             * So we cache the weak_count before calling Destory() to ensure that we can safely check if the control block is deallocated.
             */
            size_t weak_count = this->weak_count;
            Destory();
            if (weak_count == 0) {
                Deallocate();
                return true; // Indicates that the control block is deallocated
            }
        }
        return false; // Indicates that the control block is still alive
    }
    void WeakRef() noexcept { ++weak_count; }
    bool WeakUnref() {
        if (--weak_count == 0 && shared_count == 0) {
            Deallocate();
            return true; // Indicates that the control block is deallocated
        }
        return false; // Indicates that the control block is still alive
    }
};

template <typename T, template <typename> typename AtomicType = std::atomic>
class ControlBlock : public ControlBlockBase<AtomicType> {
public:
    using Pointer = RawPtr<T>;
    Pointer ptr;
    
    ControlBlock(Pointer p): ptr(p) {};
    
    void Destory() override {
        delete this->ptr.template Cast<T>().Get(); // Destroy the object pointed to by ptr
    }

    void Deallocate() override {
        delete this; // Deallocate the control block itself
    }
};

template <typename T, typename Deleter, template <typename> typename AtomicType = std::atomic>
requires InvokeAble<Deleter, T*>
class ControlBlockDeleter : public ControlBlockBase<AtomicType> {
public:
    using Pointer = RawPtr<T>;
    Pointer ptr;
    Deleter deleter;

    ControlBlockDeleter(Pointer p, Deleter d): ptr(p), deleter(std::move(d)) {};

    void Destory() override {
        deleter(this->ptr.template Cast<T>().Get());
    }

    void Deallocate() override {
        delete this; // Deallocate the control block itself
    }
};

template <typename T, typename Deleter, typename Allocator, template <typename> typename AtomicType = std::atomic>
class ControlBlockDeleterAllocator : public ControlBlockBase<AtomicType> {
public:
    using Pointer = RawPtr<T>;
    Pointer ptr;
    Deleter deleter;
    Allocator allocator;

    ControlBlockDeleterAllocator(Pointer p, Deleter d, Allocator a):
        ptr(p), deleter(std::move(d)), allocator(std::move(a)) {};

    void Destory() override {
        deleter(this->ptr);
    }

    void Deallocate() override {
        allocator.destroy(this);
        allocator.deallocate(this, 1);
    }
};

template <typename T>
struct ExtractEnableSharedFromThis {
    using TClean = RemoveCV<T>;

    template <typename U>
    inline static std::type_identity<U> test(EnableSharedFromThis<U>*);

    inline static std::type_identity<void> test(...);
public:
    using type = typename decltype(test(static_cast<TClean*>(nullptr)))::type;
    inline constexpr static bool value = !Same<type, void> && DerivedFrom<T, type>;
};

}

template <typename T>
using ExtractEnableSharedFromThis = EnableIf<detail::ExtractEnableSharedFromThis<T>::value, typename detail::ExtractEnableSharedFromThis<T>::type>;

/// @brief Enable the use of shared pointers from this pointer
template <typename T>
class EnableSharedFromThis {
    template <typename U>
    friend class SharedPtr;
private:
    mutable WeakPtr<T> m_weakThis = nullptr;
public:
    inline EnableSharedFromThis() = default;
    inline EnableSharedFromThis(const EnableSharedFromThis&) = default;
    inline EnableSharedFromThis(EnableSharedFromThis&&) noexcept = default;
    inline EnableSharedFromThis& operator=(const EnableSharedFromThis&) = default;
    inline EnableSharedFromThis& operator=(EnableSharedFromThis&&) noexcept = default;

protected:
    inline bool IsThisExpired() const noexcept {
        return m_weakThis.Expired();
    }

    inline auto GetThisWeakCount() const noexcept {
        return m_weakThis.UseCount();
    }

    /// @brief Get a shared pointer to this object
    inline SharedPtr<T> SharedFromThis() {
        if (m_weakThis.Expired()) throw BadWeakPtr();
        return m_weakThis.Lock();
    }

    /// @brief Get a shared pointer to this object, const version
    inline SharedPtr<const T> SharedFromThis() const {
        if (m_weakThis.Expired()) throw BadWeakPtr();
        return m_weakThis.Lock().template ConstCast<const T>();
    }
};

/// @brief Shared pointer
/// @tparam T The type of the pointer
template <typename T>
class SharedPtr {
    template <typename U>
    friend class SharedPtr;
    template <typename U>
    friend class WeakPtr;
    template <typename U, typename... Args>
    friend SharedPtr<U> MakePtr(Args&&... args);
protected:
    RawPtr<T> m_tptr;
    RawPtr<detail::ControlBlockBase<>> m_cb = nullptr;
    inline SharedPtr(RawPtr<T> tptr, RawPtr<detail::ControlBlockBase<>> cb): m_tptr(tptr), m_cb(cb) { if (cb) m_cb->Ref(); }
public:
    // Types
    using Type = T;
    // factory methods
    inline constexpr SharedPtr() = default;
    inline constexpr SharedPtr(std::nullptr_t): m_tptr(nullptr), m_cb(nullptr) {}

    inline constexpr SharedPtr(RawPtr<T> ptr): m_tptr(ptr), m_cb(new detail::ControlBlock<T>(ptr)) {
        if constexpr (detail::ExtractEnableSharedFromThis<T>::value){
            using Extract = ExtractEnableSharedFromThis<T>;
            // If T is derived from EnableSharedFromThis, we need to set the weak pointer
            ptr.template Cast<EnableSharedFromThis<Extract>>()->m_weakThis = WeakPtr<Extract>(m_tptr.template Cast<Extract>(), m_cb);
        }
    }
    template <typename Deleter>
    requires InvokeAble<Deleter, T*>
    inline constexpr SharedPtr(RawPtr<T> ptr, Deleter deleter): m_tptr(ptr), m_cb(new detail::ControlBlockDeleter<T, Deleter>(ptr, std::move(deleter))) {
        if constexpr (detail::ExtractEnableSharedFromThis<T>::value) {
            using Extract = ExtractEnableSharedFromThis<T>;
            ptr.template Cast<EnableSharedFromThis<Extract>>()->m_weakThis = WeakPtr<Extract>(m_tptr.template Cast<Extract>(), m_cb);
        }
    }
    template <typename Deleter, typename Allocator>
    requires InvokeAble<Deleter, T*>
    inline constexpr SharedPtr(RawPtr<T> ptr, Deleter deleter, Allocator allocator)
        : m_tptr(ptr), m_cb(new detail::ControlBlockDeleterAllocator<T, Deleter, Allocator>(ptr, std::move(deleter), std::move(allocator))) {
            if constexpr (detail::ExtractEnableSharedFromThis<T>::value){
                using Extract = ExtractEnableSharedFromThis<T>;
                ptr.template Cast<EnableSharedFromThis<Extract>>()->m_weakThis = WeakPtr<Extract>(m_tptr.template Cast<Extract>(), m_cb);
            }
        }
    
    template <typename U>
    requires ((DerivedFrom<U, T> || Void<T>) && !Same<U, T>)
    inline constexpr SharedPtr(RawPtr<U> ptr): m_tptr(ptr.template Cast<T>()), m_cb(new detail::ControlBlock<U>(m_tptr.template Cast<U>())) {
        if constexpr (detail::ExtractEnableSharedFromThis<U>::value) {
            using Extract = ExtractEnableSharedFromThis<U>;
            ptr.template Cast<EnableSharedFromThis<Extract>>()->m_weakThis = WeakPtr<Extract>(m_tptr.template Cast<Extract>(), m_cb);
        }
    }
    template <typename U, typename Deleter>
    requires ((DerivedFrom<U, T> || Void<T>) && !Same<U, T>)
    inline constexpr SharedPtr(RawPtr<U> ptr, Deleter deleter): m_tptr(ptr.template Cast<T>()), m_cb(new detail::ControlBlockDeleter<U, Deleter>(m_tptr.template Cast<U>(), std::move(deleter))) {
        if constexpr (detail::ExtractEnableSharedFromThis<U>::value){
            using Extract = ExtractEnableSharedFromThis<U>;
            ptr.template Cast<EnableSharedFromThis<Extract>>()->m_weakThis = WeakPtr<Extract>(m_tptr.template Cast<Extract>(), m_cb);
        }
    }
    template <typename U, typename Deleter, typename Allocator>
    requires ((DerivedFrom<U, T> || Void<T>) && !Same<U, T>)
    inline constexpr SharedPtr(RawPtr<U> ptr, Deleter deleter, Allocator allocator)
        : m_tptr(ptr.template Cast<T>()), m_cb(new detail::ControlBlockDeleterAllocator<U, Deleter, Allocator>(m_tptr.template Cast<U>(), std::move(deleter), std::move(allocator))) {
            if constexpr (detail::ExtractEnableSharedFromThis<U>::value){
                using Extract = ExtractEnableSharedFromThis<U>;
                ptr.template Cast<EnableSharedFromThis<Extract>>()->m_weakThis = WeakPtr<Extract>(m_tptr.template Cast<Extract>(), m_cb);
            }
    }

    template <typename U>
    requires ((DerivedFrom<U, T> || Void<T>) && !Same<U, T>)
    inline constexpr SharedPtr(U* ptr): SharedPtr(RawPtr<U>(ptr)) {}
    template <typename U, typename Deleter>
    requires ((DerivedFrom<U, T> || Void<T>) && !Same<U, T>)
    inline constexpr SharedPtr(U* ptr, Deleter deleter): SharedPtr(RawPtr<U>(ptr), std::move(deleter)) {}
    template <typename U, typename Deleter, typename Allocator>
    requires ((DerivedFrom<U, T> || Void<T>) && !Same<U, T>)
    inline constexpr SharedPtr(U* ptr, Deleter deleter, Allocator allocator): SharedPtr(RawPtr<U>(ptr), std::move(deleter), std::move(allocator)) {}

    inline SharedPtr(const SharedPtr<T>& other) noexcept: m_tptr(other.m_tptr), m_cb(other.m_cb) {
        if (m_cb) m_cb->Ref();
    }
    inline SharedPtr(SharedPtr<T>&& other) noexcept: m_tptr(std::move(other.m_tptr)), m_cb(std::move(other.m_cb)) {
        other.m_tptr = nullptr;
        other.m_cb = nullptr;
    }
    template <typename U>
    requires DerivedFrom<U, T> || Void<T>
    inline SharedPtr(const SharedPtr<U>& other) noexcept: m_tptr(other.m_tptr.template Cast<T>()), m_cb(other.m_cb) {
        if (m_cb) m_cb->Ref();
    }
    template <typename U>
    requires DerivedFrom<U, T> || Void<T>
    inline SharedPtr(SharedPtr<U>&& other) noexcept: m_tptr(other.m_tptr.template Cast<T>()), m_cb(other.m_cb) {
        other.m_tptr = nullptr;
        other.m_cb = nullptr;
    }
    inline ~SharedPtr() {
        if (m_cb) {
            m_cb->Unref();
            m_cb = nullptr; // Prevent dangling pointer
            m_tptr = nullptr;
        }
    }

    // operators
    inline SharedPtr<T>& operator=(const SharedPtr<T>& other) noexcept {
        if (this != &other) {
            if (m_cb) m_cb->Unref();
            m_tptr = other.m_tptr;
            m_cb = other.m_cb;
            if (m_cb) m_cb->Ref();
        }
        return *this;
    }
    inline SharedPtr<T>& operator=(SharedPtr<T>&& other) noexcept {
        if (this != &other) {
            if (m_cb) m_cb->Unref();
            m_tptr = std::move(other.m_tptr);
            m_cb = std::move(other.m_cb);
            other.m_tptr = nullptr;
            other.m_cb = nullptr;
        }
        return *this;
    }
    template <typename U>
    requires (DerivedFrom<U, T> && !Same<U, T>) || Void<T>
    inline SharedPtr<T>& operator=(const SharedPtr<U>& other) noexcept {
        if (m_cb) m_cb->Unref();
        m_tptr = other.m_tptr.template Cast<T>();
        m_cb = other.m_cb;
        if (m_cb) m_cb->Ref();
        return *this;
    }
    template <typename U>
    requires (DerivedFrom<U, T> && !Same<U, T>) || Void<T>
    inline SharedPtr<T>& operator=(SharedPtr<U>&& other) noexcept {
        if (m_cb) m_cb->Unref();
        m_tptr = other.m_tptr.template Cast<T>();
        m_cb = std::move(other.m_cb);
        other.m_tptr = nullptr;
        other.m_cb = nullptr;
        return *this;
    }

    inline T* operator->() const noexcept {
        _LCORE_CHECK_PTR_NOTZERO(m_tptr.Get());
        return m_tptr.operator->();
    }
    inline auto& operator*() const noexcept requires (!Void<T>) {
        _LCORE_CHECK_PTR_NOTZERO(m_tptr.Get());
        return m_tptr.operator*();
    }

    inline operator bool() const noexcept { return m_tptr != nullptr; }
    inline int operator<=>(const SharedPtr<T>& other) const noexcept {
        return m_tptr <=> other.m_tptr;
    }
    inline bool operator<(const SharedPtr<T>& other) const noexcept {
        return m_tptr < other.m_tptr;
    }
    inline bool operator<=(const SharedPtr<T>& other) const noexcept {
        return m_tptr <= other.m_tptr;
    }
    inline bool operator>(const SharedPtr<T>& other) const noexcept {
        return m_tptr > other.m_tptr;
    }
    inline bool operator>=(const SharedPtr<T>& other) const noexcept {
        return m_tptr >= other.m_tptr;
    }
    inline bool operator==(std::nullptr_t) const noexcept { return m_tptr == nullptr; }
    inline bool operator!=(std::nullptr_t) const noexcept { return m_tptr != nullptr; }
    
    // Interface methods

    inline constexpr bool IsConst() const noexcept { return std::is_const_v<T>; }
    
    /// @brief Get the raw pointer
    /// @return RawPtr<T> The raw pointer
    inline RawPtr<T> Get() const noexcept {
        return m_tptr;
    }

    /// @brief Reset the shared pointer
    inline void Reset() noexcept {
        if (m_cb) {
            m_cb->Unref();
            m_tptr = nullptr;
            m_cb = nullptr;
        }
    }

    /// @brief Get the use count of the shared pointer
    inline size_t UseCount() const noexcept {
        return m_cb ? size_t(m_cb->shared_count) : 0;
    }

    /// @brief Swap the shared pointer with another
    inline void Swap(SharedPtr<T>& other) noexcept {
        std::swap(m_tptr, other.m_tptr);
        std::swap(m_cb, other.m_cb);
    }

    // Cast methods
    /// @brief Static cast the pointer
    template <typename U>
    requires Castable<T, U>
    inline SharedPtr<U> Cast() const noexcept {
        return SharedPtr<U>(m_tptr.template Cast<U>(), m_cb);
    }

    /// @brief Dynamic cast the pointer
    template <typename U>
    inline SharedPtr<U> DynamicCast() const noexcept {
        auto realPtr = m_tptr.template DynamicCast<U>();
        if (realPtr) {
            return SharedPtr<U>(realPtr, m_cb);
        }
        return SharedPtr<U>(nullptr);
    }

    /// @brief Const cast the pointer
    template <typename U>
    requires ConstCastable<T, U>
    inline SharedPtr<U> ConstCast() const noexcept {
        return SharedPtr<U>(m_tptr.template ConstCast<U>(), m_cb);
    }

    /// @brief Reinterpret cast the pointer
    template <typename U>
    inline SharedPtr<U> ReinterpretCast() const noexcept {
        return SharedPtr<U>(m_tptr.template ReinterpretCast<U>(), m_cb);
    }
};

/// @brief Weak pointer
template <typename T>
class WeakPtr {
    template <typename U>
    friend class WeakPtr;
    template <typename U>
    friend class SharedPtr;
protected:
    RawPtr<T> m_tptr;
    RawPtr<detail::ControlBlockBase<>> m_cb = nullptr;
    inline WeakPtr(RawPtr<T> tptr, RawPtr<detail::ControlBlockBase<>> cb): m_tptr(tptr), m_cb(cb) {
        if (m_cb) m_cb->WeakRef();
    }
public:
    // Types
    using Type = T;
    // factory methods
    inline WeakPtr() = default;
    inline WeakPtr(std::nullptr_t): m_tptr(nullptr), m_cb(nullptr) {}
    inline WeakPtr(const WeakPtr<T>& other) noexcept: m_tptr(other.m_tptr), m_cb(other.m_cb) {
        if (m_cb) m_cb->WeakRef();
    }
    inline WeakPtr(WeakPtr<T>&& other) noexcept: m_tptr(std::move(other.m_tptr)), m_cb(std::move(other.m_cb)) {
        other.m_tptr = nullptr;
        other.m_cb = nullptr;
    }
    inline WeakPtr(const SharedPtr<T>& other) noexcept: m_tptr(other.m_tptr), m_cb(other.m_cb) {
        if (m_cb) m_cb->WeakRef();
    }
    template <typename U>
    requires DerivedFrom<U, T>
    inline WeakPtr(const WeakPtr<U>& other) noexcept: m_tptr(other.m_tptr.template Cast<T>()), m_cb(other.m_cb) {
        if (m_cb) m_cb->WeakRef();
    }
    template <typename U>
    requires DerivedFrom<U, T> || Void<T>
    inline WeakPtr(WeakPtr<U>&& other) noexcept: m_tptr(other.m_tptr.template Cast<T>()), m_cb(other.m_cb) {
        other.m_tptr = nullptr;
        other.m_cb = nullptr;
    }
    template <typename U>
    requires DerivedFrom<U, T> || Void<T>
    inline WeakPtr(const SharedPtr<U>& other) noexcept: m_tptr(other.m_tptr.template Cast<T>()), m_cb(other.m_cb) {
        if (m_cb) m_cb->WeakRef();
    }
    inline ~WeakPtr() {
        if (m_cb) {
            m_cb->WeakUnref();
            m_cb = nullptr; // Prevent dangling pointer
            m_tptr = nullptr;
        }
    }
    // operators
    inline WeakPtr<T>& operator=(const WeakPtr<T>& other) noexcept {
        if (this != &other) {
            if (m_cb) m_cb->WeakUnref();
            m_tptr = other.m_tptr;
            m_cb = other.m_cb;
            if (m_cb) m_cb->WeakRef();
        }
        return *this;
    }
    inline WeakPtr<T>& operator=(WeakPtr<T>&& other) noexcept {
        if (this != &other) {
            if (m_cb) m_cb->WeakUnref();
            m_tptr = std::move(other.m_tptr);
            m_cb = std::move(other.m_cb);
            other.m_tptr = nullptr;
            other.m_cb = nullptr;
        }
        return *this;
    }
    template <typename U>
    requires (DerivedFrom<U, T> && !Same<U, T>) || Void<T>
    inline WeakPtr<T>& operator=(const WeakPtr<U>& other) noexcept {
        if (m_cb) m_cb->WeakUnref();
        m_tptr = other.m_tptr.template Cast<T>();
        m_cb = other.m_cb;
        if (m_cb) m_cb->WeakRef();
        return *this;
    }
    template <typename U>
    requires (DerivedFrom<U, T> && !Same<U, T>) || Void<T>
    inline WeakPtr<T>& operator=(WeakPtr<U>&& other) noexcept {
        if (m_cb) m_cb->WeakUnref();
        m_tptr = other.m_tptr.template Cast<T>();
        m_cb = std::move(other.m_cb);
        other.m_tptr = nullptr;
        other.m_cb = nullptr;
        return *this;
    }
    template <typename U>
    requires DerivedFrom<U, T>  || Void<T>
    inline WeakPtr<T>& operator=(const SharedPtr<U>& other) noexcept {
        if (m_cb) m_cb->WeakUnref();
        m_tptr = other.m_tptr.template Cast<T>();
        m_cb = other.m_cb;
        if (m_cb) m_cb->WeakRef();
        return *this;
    }

    // Interface methods
    inline bool Expired() const noexcept {
        return !m_cb || m_cb->shared_count == 0;
    }
    
    inline SharedPtr<T> Lock() const noexcept {
        if (Expired()) {
            return SharedPtr<T>(nullptr);
        }
        return SharedPtr<T>(m_tptr, m_cb);
    }

    inline constexpr bool IsConst() const noexcept { return std::is_const_v<T>; }

    inline size_t UseCount() const noexcept {
        return m_cb ? size_t(m_cb->shared_count) : 0;
    }
};

/// @brief SharedPtr alias
template <typename T>
using Ptr = SharedPtr<T>;

template <typename T, typename... Args>
inline SharedPtr<T> MakePtr(Args&&... args) {
    // return SharedPtr<T>(new T(std::forward<Args>(args)...));
    struct CbWithT: public detail::ControlBlockBase<> {
        alignas(T) unsigned char mem[sizeof(T)];
        
        CbWithT(Args&&... args) {
            new (mem) T(std::forward<Args>(args)...); // Placement new to construct T in the memory
            this->shared_count = 0; // Start with 0 for the initial shared_ptr
        }

        void Destory() override {
            T* obj = reinterpret_cast<T*>(mem);
            obj->~T(); // Call the destructor of T
        }
        void Deallocate() override {
            delete this; // Deallocate the control block itself
        }
    };
    auto mem = new char[sizeof(CbWithT)];
    try {
        auto cb = new (mem) CbWithT{std::forward<Args>(args)...};
        return SharedPtr<T>(RawPtr<T>(reinterpret_cast<T*>(cb->mem)), cb);
    } catch (...) {
        delete[] mem; // Clean up memory in case of exception
        throw;
    }
};

template <typename T, typename... Args>
inline SharedPtr<T> New(Args&&... args) {
    return MakePtr<T>(std::forward<Args>(args)...);
};

/// @brief Unique pointer
/// @tparam T The type of the pointer
/// @tparam Deleter The deleter of the pointer
template <typename T, typename Deleter = std::default_delete<T>>
class UniquePtr {
    template <typename U, typename D>
    friend class UniquePtr;
protected:
    RawPtr<T> ptr = nullptr;
    Deleter deleter;
public:
    using Type = T;
    
    // factory methods
    inline UniquePtr() = default;
    inline UniquePtr(std::nullptr_t): ptr(nullptr) {}
    inline UniquePtr(RawPtr<T> p): ptr(p) {}
    inline UniquePtr(RawPtr<T> p, Deleter d): ptr(p), deleter(std::move(d)) {}
    inline UniquePtr(const UniquePtr<T, Deleter>& other) = delete; // No copy allowed
    inline UniquePtr(UniquePtr<T, Deleter>&& other) noexcept: ptr(other.ptr), deleter(std::move(other.deleter)) {
        other.ptr = nullptr;
    }
    template <typename U, typename D>
    requires (DerivedFrom<U, T> || Void<T>)
    inline UniquePtr(UniquePtr<U, D>&& other) noexcept: ptr(other.ptr.template Cast<T>()), deleter(std::move(other.deleter)) {
        other.ptr = nullptr;
    }

    inline ~UniquePtr() { Reset(); }

    // operators
    inline UniquePtr<T, Deleter>& operator=(const UniquePtr<T, Deleter>& other) = delete; // No copy allowed
    inline UniquePtr<T, Deleter>& operator=(UniquePtr<T, Deleter>&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr = other.ptr;
            deleter = std::move(other.deleter);
            other.ptr = nullptr;
        }
        return *this;
    }
    template <typename U, typename D>
    requires (DerivedFrom<U, T> || Void<T>)
    inline UniquePtr<T, Deleter>& operator=(UniquePtr<U, D>&& other) noexcept {
        Reset();
        ptr = other.ptr.template Cast<T>();
        deleter = std::move(other.deleter);
        other.ptr = nullptr;
        return *this;
    }

    inline T* operator->() const noexcept {
        _LCORE_CHECK_PTR_NOTZERO(ptr.Get());
        return ptr.operator->();
    }
    inline auto operator*() const noexcept requires (!Void<T>) {
        _LCORE_CHECK_PTR_NOTZERO(ptr.Get());
        return *ptr;
    }

    inline operator bool() const noexcept { return ptr != nullptr; }
    inline int operator<=>(const UniquePtr<T, Deleter>& other) const noexcept {
        return ptr <=> other.ptr;
    }

    // Interface methods
    inline constexpr bool IsConst() const noexcept { return std::is_const_v<T>; }
    
    inline RawPtr<T> Get() const noexcept { return ptr; }
    
    inline void Reset() noexcept {
        if (ptr) {
            deleter(ptr.Get());   // Call the deleter
            ptr = nullptr;  // Set the pointer to nullptr
        }
    }

    inline void Swap(UniquePtr<T, Deleter>& other) noexcept {
        std::swap(ptr, other.ptr);
        std::swap(deleter, other.deleter);
    }

    /// @brief Release the ownership of the pointer and return it
    inline RawPtr<T> Release() noexcept {
        auto temp = ptr;
        ptr = nullptr;
        return temp;
    }

    // Cast methods
    // We don't implement Cast, DynamicCast, ConstCast, or ReinterpretCast for UniquePtr
    // because UniquePtr is not meant to be used polymorphically like RawPtr, SharedPtr.
    // If you need to cast, consider using SharedPtr or RawPtr instead.
    // If you really need to cast, you can use Get() to get the raw pointer and then cast it.
};

template <typename T, typename... Args>
inline constexpr UniquePtr<T> MakeUniquePtr(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
};

LCORE_NAMESPACE_END


// Hash specialization for RawPtr, SharedPtr and UniquePtr
namespace std {
template <typename T>
struct hash<LCORE_NAMESPACE_NAME::RawPtr<T>> {
    inline size_t operator()(const LCORE_NAMESPACE_NAME::RawPtr<T>& ptr) const noexcept {
        return hash<T*>()(ptr.Get());
    }
};
template <typename T>
struct hash<LCORE_NAMESPACE_NAME::SharedPtr<T>> {
    inline size_t operator()(const LCORE_NAMESPACE_NAME::SharedPtr<T>& ptr) const noexcept {
        return hash<T*>()(ptr.Get().Get());
    }
};
template <typename T, typename Deleter>
struct hash<LCORE_NAMESPACE_NAME::UniquePtr<T, Deleter>> {
    inline size_t operator()(const LCORE_NAMESPACE_NAME::UniquePtr<T, Deleter>& ptr) const noexcept {
        return hash<T*>()(ptr.Get().Get());
    }
};
}
