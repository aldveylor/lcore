/**
 * @file base.hpp
 * @author liyanes(liyanes@outlook.com)
 * @brief Base definitions for the library
 * @version 0.1
 * @date 2024-06-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once

#if __cplusplus < 202002L
#error "This library requires C++20 or later"
#endif

// #define LCORE_NAMESPACE_BEGIN namespace lcore {
// #define LCORE_NAMESPACE_END }
#include "config.h"

#ifdef LCORE_BUDEG
#include "assert.hpp"
#define CONSTEXPR_NODEBUG
#else
#define CONSTEXPR_NODEBUG constexpr
#endif

#include "traits.hpp"
#include "rawptr.hpp"
#include "class.hpp"
#include <typeindex>

LCORE_NAMESPACE_BEGIN

template <typename T>
using Ref = T&;

using TypeInfo = std::type_info;
using TypeInfoPtr = RawPtr<const TypeInfo>;
using TypeInfoRef = Ref<const TypeInfo>;

class TypeIndex: public std::type_index {
public:
    using std::type_index::type_index;
    template <typename T>
    static TypeIndex Of() {
        return TypeIndex(typeid(T));
    }
};

using Size = std::size_t;
using Offset = std::ptrdiff_t;

struct Monostate {
    inline operator bool() const noexcept {
        return false;
    }
    inline bool operator!() const noexcept {
        return true;
    }
    inline operator std::nullptr_t() const noexcept {
        return nullptr;
    }
    inline operator void*() const noexcept {
        return nullptr;
    }
    inline bool operator==( Monostate ) const noexcept {
        return true;
    }
    inline bool operator!=( Monostate ) const noexcept {
        return false;
    }
};

LCORE_NAMESPACE_END
