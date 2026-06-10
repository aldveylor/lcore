#pragma once
#include "config.h"
#include "traits.hpp"

#ifdef LCORE_DEBUG
#include "assert.hpp"
#define _LCORE_CHECK_PTR_NOTZERO(ptr) LCORE_ASSERT(ptr, "Try to dereference a null pointer")
#else
#define _LCORE_CHECK_PTR_NOTZERO(ptr) do {} while (0)
#endif

LCORE_NAMESPACE_BEGIN

template <typename T, typename U>
concept Castable = requires (T* t) {
    { static_cast<U*>(t) } -> ConvertibleTo<U*>;
};

template <typename T, typename U>
concept ConstCastable = requires (T* t) {
    { const_cast<U*>(t) } -> ConvertibleTo<U*>;
};

/// @brief Raw pointer
/// @tparam T The type of the pointer
template <typename T>
class RawPtr {
    template <typename U>
    friend class RawPtr;
protected:
    T* ptr = nullptr;
public:
    using Type = T;
    using Pointer = Type*;
    using ConstPointer = const Type*;

    inline constexpr RawPtr() = default;
    inline constexpr RawPtr(T* ptr): ptr(ptr) {}
    inline constexpr RawPtr(std::nullptr_t): ptr(nullptr) {}
    inline constexpr RawPtr(const RawPtr<T>& ptr): ptr(ptr.ptr) {}
    inline constexpr RawPtr(RawPtr<T>&& ptr): ptr(ptr.ptr) {ptr.ptr = nullptr;}
    template <typename U>
    requires (DerivedFrom<U, T> || Void<T>)
    inline constexpr RawPtr(const RawPtr<U>& ptr): ptr(ptr.ptr) {}

    inline constexpr RawPtr<T>& operator=(const RawPtr<T>& ptr) noexcept {this->ptr = ptr.ptr; return *this;}
    inline constexpr RawPtr<T>& operator=(RawPtr<T>&& ptr) noexcept {this->ptr = ptr.ptr; ptr.ptr = nullptr; return *this;}
    template <typename U>
    requires (DerivedFrom<U, T> || Void<T>)
    inline constexpr RawPtr<T>& operator=(const RawPtr<U>& ptr) noexcept { this->ptr = ptr.ptr; return *this; }
    
    inline constexpr RawPtr<T>& operator=(std::nullptr_t) noexcept {this->ptr = nullptr; return *this;}
    
    inline constexpr T* operator->() const noexcept {
        _LCORE_CHECK_PTR_NOTZERO(ptr);
        return ptr;
    }
    inline constexpr auto& operator*() const noexcept requires (!Void<T>) {
        _LCORE_CHECK_PTR_NOTZERO(ptr);
        return *ptr;
    }
    // inline constexpr void operator*() const requires Void<T> {
    //     static_assert(!Void<T>, "Cannot dereference a void pointer");
    // }

    // Conversion
    inline constexpr operator bool() const noexcept {return ptr != nullptr;}
    inline constexpr operator T*() const noexcept {return ptr;}

    // Pointer comparison
    inline constexpr auto operator<=>(const RawPtr<T>& other) const noexcept {
        return ptr <=> other.ptr;
    }
    inline constexpr auto operator==(std::nullptr_t) const noexcept { return ptr == nullptr; }
    inline constexpr auto operator!=(std::nullptr_t) const noexcept { return ptr != nullptr; }
    template <typename U>
    inline constexpr auto operator==(const RawPtr<U>& other) const noexcept { return ptr == other.ptr; }
    template <typename U>
    inline constexpr auto operator!=(const RawPtr<U>& other) const noexcept { return ptr != other.ptr; }
    template <typename U>
    inline constexpr auto operator==(const U* other) const noexcept { return ptr == other; }
    template <typename U>
    inline constexpr auto operator!=(const U* other) const noexcept { return ptr != other; }

    // Pointer Offset
    inline constexpr RawPtr<T> operator+(ptrdiff_t offset) const noexcept {
        return RawPtr<T>(ptr + offset);
    }
    inline constexpr RawPtr<T> operator-(ptrdiff_t offset) const noexcept {
        return RawPtr<T>(ptr - offset);
    }
    inline constexpr ptrdiff_t operator-(const RawPtr<T>& other) const noexcept {
        return ptr - other.ptr;
    }

    // Pointer arithmetic
    inline constexpr RawPtr<T>& operator+=(ptrdiff_t offset) noexcept {
        ptr += offset;
        return *this;
    }
    inline constexpr RawPtr<T>& operator-=(ptrdiff_t offset) noexcept {
        ptr -= offset;
        return *this;
    }
    inline constexpr RawPtr<T> operator[](ptrdiff_t offset) const noexcept {
        _LCORE_CHECK_PTR_NOTZERO(ptr);
        return RawPtr<T>(ptr + offset);
    }

    inline constexpr bool IsConst() const noexcept {return std::is_const_v<T>;}
    inline constexpr T* Get() const noexcept {return ptr;}
    inline constexpr void Delete() noexcept {
        delete ptr;
        ptr = nullptr;
    }
    inline constexpr void Reset() noexcept {
        if (ptr) {
            delete ptr;
            ptr = nullptr;
        }
    }
    inline constexpr void Swap(RawPtr<T>& other) noexcept {
        std::swap(ptr, other.ptr);
    }

    template <typename U>
    requires Castable<T, U>
    inline constexpr RawPtr<U> Cast() const noexcept {
        return static_cast<U*>(ptr);
    }

    template <typename U>
    inline constexpr RawPtr<U> DynamicCast() const noexcept {
        return dynamic_cast<U*>(ptr);
    }

    template <typename U>
    requires ConstCastable<T, U>
    inline constexpr RawPtr<U> ConstCast() const noexcept {
        return const_cast<U*>(ptr);
    }

    template <typename U>
    inline constexpr RawPtr<U> ReinterpretCast() const noexcept {
        return reinterpret_cast<U*>(ptr);
    }
};

LCORE_NAMESPACE_END

