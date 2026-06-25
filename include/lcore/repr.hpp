#pragma once
#include "lcore/traits.hpp"
#include "lcore/type.hpp"
#include <string>
#include <string_view>
#include <format>
#include "memory.hpp"

LCORE_NAMESPACE_BEGIN

/// @brief The repr function, used to convert the object to string
template <typename T>
inline std::string repr(const T& value){
    return std::format("<{} at {:p}>", demangle<T>(), &value);
};

template <>
inline std::string repr(const std::string& value){
    return std::format("\"{}\"", value);
};

template <>
inline std::string repr(const std::string_view& value){
    return std::format("\"{}\"", value);
};

template <IsNumber T>
inline std::string repr(const T& value){
    return std::to_string(value);
};

template <IsPointer T>
inline std::string repr(const T& value){
    if (value == nullptr) return std::format("<P: {:p}>->nullptr", value);
    return std::format("<P: {:p}>->{}", value, repr(*value));
};

template <typename T>
inline std::string repr(const SharedPtr<T>& value) {
    if (value == nullptr) return std::format("<SP: {:p}>->nullptr", value.Get());
    return std::format("<SP: {:p}>->{}", value.Get(), repr(*value));
}

template <typename T>
inline std::string repr(const WeakPtr<T>& value) {
    if (value.Expired()) return std::format("<WP: {:p}>->expired", value.Get());
    return std::format("<WP: {:p}>->{}", value.lock().Get(), repr(*value));
}

template <typename T>
inline std::string repr(const UniquePtr<T>& value) {
    if (value == nullptr) return std::format("<UP: {:p}>->nullptr", value.Get());
    return std::format("<UP: {:p}>->{}", value.Get(), repr(*value));
}

template <IsMap T>
inline std::string repr(const T& value){
    std::stringstream ss;
    std::size_t count = 0;
    ss << "{";
    for (const auto& [key, val]: value){
        ss << repr(key) << ": " << repr(val) << ", ";
        count++;
    }
    std::string ret = ss.str();
    if (count) ret = ret.substr(0, ret.size() - 2);
    ret += "}";
    return ret;
};

template <ConstIterable T>
inline std::string repr(const T& value){
    std::stringstream ss;
    std::size_t count = 0;
    ss << "[";
    for (const auto& val: value){
        ss << repr(val) << ", ";
        count++;
    }
    std::string ret = ss.str();
    if (count) ret = ret.substr(0, ret.size() - 2);
    ret += "]";
    return ret;
};

LCORE_NAMESPACE_END
