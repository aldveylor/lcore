#pragma once
#include "logger.hpp"
#include "repr.hpp"

LCORE_NAMESPACE_BEGIN

namespace _detail {

/// @brief The repr function, used to convert the object to string
/// @tparam ...Args The type of the object
/// @param ...args The object
/// @return The string representation of the object
/// @note This function is used for logging purpose
/// @note User code should not use this function directly, use repr() if needed
template <typename ...Args>
inline std::vector<std::string> __LogRepr(Args&&... args){
    return {repr(args)...};
}

/// @brief Join a list of names and parameters into a string
/// @param names The parameter names
/// @param args The parameter values
/// @return The joined string
template <size_t N>
inline std::string __LogJoin(std::array<std::string_view, N>&& names, std::vector<std::string>&& args){
    std::stringstream ss;
    if (names.size() == 1 && names[0].empty()) return "";
    for (size_t i = 0; i < names.size(); i++){
        ss << names[i] << "=" << args[i] << ", ";
    }
    std::string ret = ss.str();
    return ret.substr(0, ret.size() - 2);
}

template <size_t N>
inline constexpr std::array<std::string_view, N> __LogSplit(const char* value){
    size_t len = 0;
    while (value[len] != '\0') len++;
    if (len == 0) return {};
    else {
        std::array<std::string_view, N> ret;
        size_t bracket = 0;
        size_t pos = 0;
        size_t index = 0;
        for (size_t i = 0; i < len; i++){
            if (value[i] == '(' || value[i] == '[' || value[i] == '{'){
                bracket++;
            }else if (value[i] == ')' || value[i] == ']' || value[i] == '}'){
                bracket--;
            }
            if (value[i] == ',' && bracket == 0){
                ret[index++] = std::string_view(value + pos, i - pos);
                pos = i + 1;
            }
        }
        ret[index] = std::string_view(value + pos, len - pos);
        return ret;
    }
}

}


LCORE_NAMESPACE_END

#define LOG_PARAMETERS(...) \
    LOG_DEBUG(std::format("({})", LCORE_NAMESPACE::_detail::__LogJoin<GET_ARG_COUNT(__VA_ARGS__)>( \
        LCORE_NAMESPACE::_detail::__LogSplit<GET_ARG_COUNT(__VA_ARGS__)>(#__VA_ARGS__), \
        LCORE_NAMESPACE::_detail::__LogRepr(__VA_ARGS__))))

#define LOG_FUNCTION(...) \
    LOG_DEBUG(std::format("{}({})", __FUNCTION__, LCORE_NAMESPACE::_detail::__LogJoin<GET_ARG_COUNT(__VA_ARGS__)>( \
        LCORE_NAMESPACE::_detail::__LogSplit<GET_ARG_COUNT(__VA_ARGS__)>(#__VA_ARGS__), \
        LCORE_NAMESPACE::_detail::__LogRepr(__VA_ARGS__))))
