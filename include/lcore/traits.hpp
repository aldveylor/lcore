#pragma once
#include "config.h"
#include <type_traits>
#include <tuple>
#include <cstddef>

LCORE_NAMESPACE_BEGIN

template <typename T>
using Forward = T;

template <typename T>
concept Number = std::is_arithmetic_v<T>;

template <typename T>
concept Integer = std::is_integral_v<T>;

template <typename T>
concept Floating = std::is_floating_point_v<T>;

template <typename T>
concept Unsigned = std::is_unsigned_v<T>;

template <typename T>
concept Signed = std::is_signed_v<T>;

template <typename T>
concept Real = Floating<T> || Integer<T>;

template <typename T>
concept Void = std::is_void_v<T>;

template <typename T, typename ...Args>
concept Same = (std::is_same_v<T, Args> && ...);

template <typename T, typename ...Items>
concept OneOf = (Same<T, Items> || ...);

template <typename T>
concept Pointer = std::is_pointer_v<T>;

template <auto T, auto U>
concept Equal = Same<decltype(T), decltype(U)> && T == U;

template <auto T>
concept TrueType = std::bool_constant<T>::value;

template <typename T>
using RemoveReference = std::remove_reference_t<T>;

template <typename T>
using RemoveConst = std::remove_const_t<T>;

template <typename T>
using RemoveVolatile = std::remove_volatile_t<T>;

template <typename T>
using RemoveCV = std::remove_cv_t<T>;

template <typename T>
concept Reference = std::is_reference_v<T>;

template <typename Func, typename ...Args>
concept IsCallable = requires(Func func, Args... args){
    { func(args...) };
};

template <typename Func, typename ...Args>
concept InvokeAble = IsCallable<Func, Args...>;

template <typename Func, typename ...Args>
using ResultCallable = decltype(std::declval<Func>()(std::declval<Args>()...));

template <typename Func, typename ArgsTuple>
using ResultCallableOfTuple = decltype(std::apply(std::declval<Func>(), std::declval<ArgsTuple>()));

template <typename T>
concept Const = std::is_const_v<T>;

template <typename T>
concept Volatile = std::is_volatile_v<T>;

template <typename T>
concept IsClass = std::is_class_v<T>;

template <typename T>
concept IsUnion = std::is_union_v<T>;

/**
 * Iterator Traits, see also in std::ranges::iterator_traits
*/

/// @brief Concept for checking if a type is iterable
/// @tparam T The type to be checked
template <typename T>
concept Iterable = requires(T t){
    t.begin();
    t.end();
    t.begin() == t.end();
    t.begin() != t.end();
};

/// @brief Get the value type of an iterable
/// @tparam T The iterable type
template <typename T>
using IterableValueType = RemoveCV<RemoveReference<decltype(*std::declval<T>().begin())>>;

template <typename T>
concept ConstIterable = requires(const T t){
    requires Iterable<T>;
    requires Const<IterableValueType<T>>;
};


/// @brief Concept for checking if a type is a map
/// @tparam T The type to be checked
template <typename T>
concept IsMap = requires(T t){
    requires Iterable<T>;
    {IterableValueType<T>::first} -> std::same_as<typename T::key_type>;
    {IterableValueType<T>::second} -> std::same_as<typename T::mapped_type>;
};

/// @brief Get the key type of a map
/// @tparam T The map type
template <typename T>
using MapKeyType = typename T::key_type;

/// @brief Get the value type of a map
/// @tparam T The map type
template <typename T>
using MapValueType = typename T::mapped_type;

template <typename Func, typename ...Args>
using ReturnType = decltype(std::declval<Func>()(std::declval<Args>()...));

template <typename T>
concept StringLike = requires(T t){
    requires Iterable<T>;
    requires Same<IterableValueType<T>, char>;
    {t.c_str()} -> OneOf<const char*, const char[]>;
    {t.size()} -> Same<size_t>;
};

template <typename T>
concept StandardLayout = std::is_standard_layout_v<T>;

template <typename T>
concept Trivial = std::is_trivial_v<T>;

/// @brief Simple type (not class or pointer or reference)
/// @tparam T The type to be checked
template <typename T>
concept SimpleType = !std::is_class_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T>;

/// @brief Enum type
/// @tparam T The type to be checked
template <typename T>
concept IsEnum = std::is_enum_v<T>;

/// @brief Underlying type of an enum
/// @tparam T The enum type
template <typename T>
using UnderlyingType = std::underlying_type_t<T>;

template <typename T, typename Base>
concept DerivedFrom = std::is_base_of_v<Base, T>;

template <typename T, typename U>
concept CanConvTo = std::is_convertible_v<T, U>;

template <typename T, typename U>
concept ConvertibleTo = std::convertible_to<T, U>;

#define GetMemberType(ParentType, MemberName) (decltype(std::declval<ParentType>.##MemberName))

/// @brief Get the Nth type of a type list
/// @tparam index The index of the type
/// @tparam Args The type list
template <size_t index, typename ...Args>
using NthType = std::tuple_element_t<index, std::tuple<Args...>>;

template <size_t index, typename Tuple>
using NthTypeOfTuple = std::tuple_element_t<index, Tuple>;

namespace detail {

template <typename T, typename ...Args>
struct _IndexOf;

template <typename T, typename ...Args>
struct _IndexOf<T, T, Args...>: std::integral_constant<size_t, 0> {};

template <typename T, typename U, typename ...Args>
struct _IndexOf<T, U, Args...>: std::integral_constant<size_t, 1 + _IndexOf<T, Args...>::value> {};

};

/// @brief Get the index of a type in a type list
/// @tparam T The type to be found
/// @tparam Args The type list
template <typename T, typename ...Args>
inline constexpr size_t IndexOf = detail::_IndexOf<T, Args...>::value;

template <typename T, typename U>
concept Comparable = requires(T t, U u){
    {t == u} -> Same<bool>;
    {t != u} -> Same<bool>;
    {t < u} -> Same<bool>;
    {t > u} -> Same<bool>;
    {t <= u} -> Same<bool>;
    {t >= u} -> Same<bool>;
};

template <typename T, typename U>
concept EqualComparable = requires(T t, U u){
    {t == u} -> Same<bool>;
    {t != u} -> Same<bool>;
};

template <bool Cond, typename T = void>
using EnableIf = std::enable_if_t<Cond, T>;

template <bool Cond, typename T, typename F>
using Conditional = std::conditional_t<Cond, T, F>;

template <typename I, typename M, typename F>
using ReplaceIf = std::conditional_t<std::is_same_v<I, M>, F, I>;

template <typename T, typename ...Args>
concept ConstructibleWith = requires(T t, Args... args){
    {T(args...)} -> Same<T>;
};

template <typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

template <typename T>
concept CopyConstructible = std::is_copy_constructible_v<T>;

template <typename T>
concept MoveConstructible = std::is_move_constructible_v<T>;

template <typename T>
concept CopyAssignable = std::is_copy_assignable_v<T>;

template <typename T>
concept MoveAssignable = std::is_move_assignable_v<T>;

template <typename T>
concept Destructible = std::is_destructible_v<T>;

// RemoveNthOfTuple

namespace detail {
template <size_t index, typename Tuple>
struct _RemoveNthOfTuple;

template <size_t index, typename ...Args>
struct _RemoveNthOfTuple<index, std::tuple<Args...>>{
    using type = typename _RemoveNthOfTuple<index - 1, std::tuple<Args...>>::type;
};

template <typename First, typename ...Args>
struct _RemoveNthOfTuple<0, std::tuple<First, Args...>>{
    using type = std::tuple<Args...>;
};
}

template <size_t index, typename Tuple>
using RemoveNthOfTuple = typename detail::_RemoveNthOfTuple<index, Tuple>::type;

/// ========== Function traits =========

template <typename T>
struct FunctionTraits;

template <typename Ret, typename ...Args>
struct FunctionTraits<Ret(Args...)> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    using ArgsDecayTuple = std::tuple<std::decay_t<Args>...>;
    template <size_t index>
    using NthParameterType = NthType<index, Args...>;

    static constexpr size_t Arity = sizeof...(Args);
    static constexpr bool IsCallable = true;
    static constexpr bool IsFunction = true;
    static constexpr bool IsMemberFunction = false;
    static constexpr bool IsFunctionPointer = false;
    static constexpr bool IsFunctionReference = false;
    static constexpr bool IsMemberFunctionPointer = false;
    static constexpr bool IsCallableObject = false;
};

template <typename Ret, typename ...Args>
struct FunctionTraits<Ret(*)(Args...)> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    using ArgsDecayTuple = std::tuple<std::decay_t<Args>...>;
    template <size_t index>
    using NthParameterType = NthType<index, Args...>;

    static constexpr size_t Arity = sizeof...(Args);
    static constexpr bool IsCallable = true;
    static constexpr bool IsFunction = false;
    static constexpr bool IsMemberFunction = false;
    static constexpr bool IsFunctionPointer = true;
    static constexpr bool IsFunctionReference = false;
    static constexpr bool IsMemberFunctionPointer = false;
    static constexpr bool IsCallableObject = false;
};

template <typename Ret, typename ...Args>
struct FunctionTraits<Ret(&)(Args...)> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    using ArgsDecayTuple = std::tuple<std::decay_t<Args>...>;
    template <size_t index>
    using NthParameterType = NthType<index, Args...>;

    static constexpr size_t Arity = sizeof...(Args);
    static constexpr bool IsCallable = true;
    static constexpr bool IsFunction = false;
    static constexpr bool IsMemberFunction = false;
    static constexpr bool IsFunctionPointer = false;
    static constexpr bool IsFunctionReference = false;
    static constexpr bool IsMemberFunctionPointer = false;
    static constexpr bool IsCallableObject = true;
};

template <typename Class, typename Ret, typename ...Args>
struct FunctionTraits<Ret(Class::*)(Args...)> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    using ArgsDecayTuple = std::tuple<std::decay_t<Args>...>;
    template <size_t index>
    using NthParameterType = NthType<index, Args...>;
    using ClassType = Class;

    static constexpr size_t Arity = sizeof...(Args);
    static constexpr bool IsCallable = true;
    static constexpr bool IsFunction = false;
    static constexpr bool IsMemberFunction = true;
    static constexpr bool IsFunctionPointer = false;
    static constexpr bool IsFunctionReference = false;
    static constexpr bool IsMemberFunctionPointer = true;
    static constexpr bool IsCallableObject = false;

    static constexpr bool IsConst = false;
};

template <typename Class, typename Ret, typename ...Args>
struct FunctionTraits<Ret(Class::*)(Args...) const> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    using ArgsDecayTuple = std::tuple<std::decay_t<Args>...>;
    template <size_t index>
    using NthParameterType = NthType<index, Args...>;
    using ClassType = Class;

    static constexpr size_t Arity = sizeof...(Args);
    static constexpr bool IsCallable = true;
    static constexpr bool IsFunction = false;
    static constexpr bool IsMemberFunction = true;
    static constexpr bool IsFunctionPointer = false;
    static constexpr bool IsFunctionReference = false;
    static constexpr bool IsMemberFunctionPointer = true;
    static constexpr bool IsCallableObject = false;

    static constexpr bool IsConst = true;
};

template <typename T>
struct FunctionTraits: FunctionTraits<decltype(&T::operator())> {};

/// ====================================

// /// ======== Character Traits ========

// template <typename T>
// class CharTraits;

// template <>
// class CharTraits<char> {
// public:
//     using CharType = char;
//     static constexpr size_t size = sizeof(CharType);

//     using PosType = std::streampos;
//     using OffType = std::streamoff;
//     using IntType = int;

    
// };

// /// ====================================

namespace detail {

template <typename T>
struct _ExtractMemberType;

template <typename T, typename U>
struct _ExtractMemberType<T U::*> {
    using type = T;
};

}

template <typename T>
using ExtractMemberType = typename detail::_ExtractMemberType<T>::type;


LCORE_NAMESPACE_END
