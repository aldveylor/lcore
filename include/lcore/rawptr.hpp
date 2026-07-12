#pragma once
#include "config.h"
#include "traits.hpp"

#ifdef LCORE_DEBUG
#include "assert.hpp"
#include "type.hpp"
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
    T* m_tptr = nullptr;
public:
    using Type = T;
    using Pointer = Type*;
    using ConstPointer = const Type*;

    inline constexpr RawPtr() = default;
    inline constexpr RawPtr(T* m_tptr): m_tptr(m_tptr) {}
    inline constexpr RawPtr(std::nullptr_t): m_tptr(nullptr) {}
    inline constexpr RawPtr(const RawPtr<T>& m_tptr): m_tptr(m_tptr.m_tptr) {}
    inline constexpr RawPtr(RawPtr<T>&& m_tptr): m_tptr(m_tptr.m_tptr) {m_tptr.m_tptr = nullptr;}
    template <typename U>
    requires (DerivedFrom<U, T> || Void<T>)
    inline constexpr RawPtr(const RawPtr<U>& m_tptr): m_tptr(m_tptr.m_tptr) {}

    inline constexpr RawPtr<T>& operator=(const RawPtr<T>& m_tptr) noexcept {this->m_tptr = m_tptr.m_tptr; return *this;}
    inline constexpr RawPtr<T>& operator=(RawPtr<T>&& m_tptr) noexcept {this->m_tptr = m_tptr.m_tptr; m_tptr.m_tptr = nullptr; return *this;}
    template <typename U>
    requires (DerivedFrom<U, T> || Void<T>)
    inline constexpr RawPtr<T>& operator=(const RawPtr<U>& m_tptr) noexcept { this->m_tptr = m_tptr.m_tptr; return *this; }
    
    inline constexpr RawPtr<T>& operator=(std::nullptr_t) noexcept {this->m_tptr = nullptr; return *this;}
    
    inline constexpr T* operator->() const noexcept {
        _LCORE_CHECK_PTR_NOTZERO(m_tptr);
        return m_tptr;
    }
    inline constexpr auto& operator*() const noexcept requires (!Void<T>) {
        _LCORE_CHECK_PTR_NOTZERO(m_tptr);
        return *m_tptr;
    }
    // inline constexpr void operator*() const requires Void<T> {
    //     static_assert(!Void<T>, "Cannot dereference a void pointer");
    // }

    // Conversion
    inline constexpr operator bool() const noexcept {return m_tptr != nullptr;}
    inline constexpr operator T*() const noexcept {return m_tptr;}

    // Pointer comparison
    inline constexpr auto operator<=>(const RawPtr<T>& other) const noexcept {
        return m_tptr <=> other.m_tptr;
    }
    inline constexpr auto operator==(std::nullptr_t) const noexcept { return m_tptr == nullptr; }
    inline constexpr auto operator!=(std::nullptr_t) const noexcept { return m_tptr != nullptr; }
    template <typename U>
    inline constexpr auto operator==(const RawPtr<U>& other) const noexcept { return m_tptr == other.m_tptr; }
    template <typename U>
    inline constexpr auto operator!=(const RawPtr<U>& other) const noexcept { return m_tptr != other.m_tptr; }
    template <typename U>
    inline constexpr auto operator==(const U* other) const noexcept { return m_tptr == other; }
    template <typename U>
    inline constexpr auto operator!=(const U* other) const noexcept { return m_tptr != other; }

    // Pointer Offset
    inline constexpr RawPtr<T> operator+(ptrdiff_t offset) const noexcept {
        return RawPtr<T>(m_tptr + offset);
    }
    inline constexpr RawPtr<T> operator-(ptrdiff_t offset) const noexcept {
        return RawPtr<T>(m_tptr - offset);
    }
    inline constexpr ptrdiff_t operator-(const RawPtr<T>& other) const noexcept {
        return m_tptr - other.m_tptr;
    }

    // Pointer arithmetic
    inline constexpr RawPtr<T>& operator+=(ptrdiff_t offset) noexcept {
        m_tptr += offset;
        return *this;
    }
    inline constexpr RawPtr<T>& operator-=(ptrdiff_t offset) noexcept {
        m_tptr -= offset;
        return *this;
    }
    inline constexpr RawPtr<T> operator[](ptrdiff_t offset) const noexcept {
        _LCORE_CHECK_PTR_NOTZERO(m_tptr);
        return RawPtr<T>(m_tptr + offset);
    }

    inline constexpr bool IsConst() const noexcept {return std::is_const_v<T>;}
    inline constexpr T* Get() const noexcept {return m_tptr;}
    inline constexpr void Delete() noexcept {
        delete m_tptr;
        m_tptr = nullptr;
    }
    inline constexpr void Reset() noexcept {
        if (m_tptr) {
            delete m_tptr;
            m_tptr = nullptr;
        }
    }
    inline constexpr void Swap(RawPtr<T>& other) noexcept {
        std::swap(m_tptr, other.m_tptr);
    }

    template <typename U>
    requires Castable<T, U>
    inline constexpr RawPtr<U> Cast() const noexcept {
#ifdef LCORE_DEBUG
        // In debug mode, use DynamicCast to check the validity of the cast
        if constexpr (IsPolymorphic<T>) {
            if (m_tptr == nullptr) return nullptr;
            U* castedPtr = dynamic_cast<U*>(m_tptr);
            LCORE_ASSERT(castedPtr, std::format("Cast(): Invalid cast from {} to {}", demangle<T>(), demangle<U>()));
            return RawPtr<U>(castedPtr);
        } else { // If T is not polymorphic, use static_cast
            return static_cast<U*>(m_tptr);
        }
#else
        return static_cast<U*>(m_tptr);
#endif
    }

    template <typename U>
    inline constexpr RawPtr<U> DynamicCast() const noexcept {
        return dynamic_cast<U*>(m_tptr);
    }

    template <typename U>
    requires ConstCastable<T, U>
    inline constexpr RawPtr<U> ConstCast() const noexcept {
        return const_cast<U*>(m_tptr);
    }

    template <typename U>
    inline constexpr RawPtr<U> ReinterpretCast() const noexcept {
        return reinterpret_cast<U*>(m_tptr);
    }

    // Helper operators (forward operators to RawPtr)
    // Member pointer
    template <typename M>
    inline decltype(auto) operator->*(M member) const noexcept requires (IsClass<T> && !Void<M>) {
        return m_tptr->*member;
    }
    template <typename M>
    inline decltype(auto) operator->*(M member) noexcept requires (IsClass<T> && !Void<M>) {
        return m_tptr->*member;
    }
    template <typename M>
    inline void operator->*(M member) const noexcept requires (IsClass<T> && Void<M>) {
        return m_tptr->*member;
    }
    template <typename M>
    inline void operator->*(M member) noexcept requires (IsClass<T> && Void<M>) {
        return m_tptr->*member;
    }
    // Subscript operator
    template <typename U>
    inline decltype(auto) operator[](U index) const noexcept requires (!Void<T>) {
        return (*m_tptr)[std::forward<U>(index)];
    }
    template <typename U>
    inline decltype(auto) operator[](U index) noexcept requires (!Void<T>) {
        return (*m_tptr)[std::forward<U>(index)];
    }
};

LCORE_NAMESPACE_END

