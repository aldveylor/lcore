#pragma once
#include "base.hpp"
#include <memory>
#include <string>
#include <cxxabi.h>

LCORE_NAMESPACE_BEGIN

/// @brief Demangle a type name
/// @param name The type name
/// @return The demangled type name
inline std::string demangle(const char* name) {
    int status = -1;
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };
    return (status==0) ? res.get() : "error occurred";
}

/// @brief Demangle a type name
/// @tparam T The type to be demangled
/// @return The demangled type name
template<typename T>
std::string demangle() {
    return demangle(typeid(T).name());
}

/// @brief Get the type of the nth type in a list of types
/// @tparam ...Args The types of the list
/// @param n The index of the type
/// @return The type of the nth type
template <typename ...Args>
inline const std::type_info& GetNthType(size_t n){
    std::array<const std::type_info*, sizeof...(Args)> types = {&typeid(Args)...};
    return *types[n];
};



LCORE_NAMESPACE_END
