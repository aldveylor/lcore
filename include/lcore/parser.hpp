#pragma once
#include "string.hpp"
#include "container/vector.hpp"
#include <charconv>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

LCORE_NAMESPACE_BEGIN

/** @brief Structural string constant usable as a C++20 non-type template argument. */
template <std::size_t N>
struct StringConstant {
    char data[N];

    constexpr StringConstant(const char (&value)[N]) noexcept {
        for (std::size_t i = 0; i < N; ++i) data[i] = value[i];
    }

    constexpr std::size_t size() const noexcept { return N - 1; }
};

/** @brief Value conversion customization point; specialize for user-defined types. */
template <typename T>
struct ParserDeserializer {
    T deserialize(StringView value) { return T(value); }
};

/** @brief Strict decimal conversion for integral and floating-point values. */
template <typename T>
requires (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
struct ParserDeserializer<T> {
    T deserialize(StringView value) {
        if (value.empty()) throw std::invalid_argument("ArgumentParser: empty number");
        T result;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        if (parsed.ec == std::errc::result_out_of_range) {
            throw std::out_of_range("ArgumentParser: number out of range");
        }
        if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
            throw std::invalid_argument("ArgumentParser: invalid number");
        }
        return result;
    }
};

/** @brief Convert the boolean spellings true, false, 1 and 0. */
template <>
struct ParserDeserializer<bool> {
    bool deserialize(StringView value) {
        if (value == "true" || value == "1") return true;
        if (value == "false" || value == "0") return false;
        throw std::invalid_argument("ArgumentParser: invalid boolean");
    }
};

namespace detail {

/** @brief Match a complete long or short option against its template name. */
template <StringConstant Name, bool Short = false>
struct ParserOptionMatcher {
    static_assert(Name.size() > 0, "Option names must not be empty");

    static constexpr bool Match(StringView argument) noexcept {
        constexpr std::size_t prefixSize = Short ? 1 : 2;
        return argument.size() == Name.size() + prefixSize
            && argument[0] == '-' && (Short || argument[1] == '-')
            && MatchesCharacters(argument, std::make_index_sequence<Name.size()>{});
    }

private:
    template <std::size_t... Indices>
    static constexpr bool MatchesCharacters(StringView argument, std::index_sequence<Indices...>) noexcept {
        return ((argument[Indices + (Short ? 1 : 2)] == Name.data[Indices]) && ...);
    }
};

/** @brief Match --name=value using a template constant for the option name. */
template <StringConstant Name, bool Positional = (Name.size() == 0)>
struct ParserArgumentMatcher {
    static constexpr bool Match(StringView argument, bool positional, StringView& value) noexcept {
        if (positional || argument.size() < Name.size() + 3
            || argument[0] != '-' || argument[1] != '-'
            || argument[Name.size() + 2] != '=') return false;
        if (!MatchesCharacters(argument, std::make_index_sequence<Name.size()>{})) return false;
        value = argument.substr(Name.size() + 3);
        return true;
    }

private:
    template <std::size_t... Indices>
    static constexpr bool MatchesCharacters(StringView argument, std::index_sequence<Indices...>) noexcept {
        return ((argument[Indices + 2] == Name.data[Indices]) && ...);
    }
};

/** @brief Partial specialization for unnamed positional argument bindings. */
template <StringConstant Name>
struct ParserArgumentMatcher<Name, true> {
    static constexpr bool Match(StringView argument, bool positional, StringView& value) noexcept {
        if (!positional) return false;
        value = argument;
        return true;
    }
};

}

/**
 * @brief Parse command-line arguments into bound references using template names.
 * @note Skips argv[0]. Arguments and bound targets must outlive parsing; targets
 *       storing StringView also borrow the argument strings. Named options use
 *       --name=value or -n value; switches use --name or -n. Registered options
 *       take priority over positional bindings, including for negative numbers.
 *       A standalone -- makes all subsequent arguments positional.
 */
class ArgumentParser {
public:
    /** @brief Construct from argc/argv; each of the argc entries must be non-null. */
    ArgumentParser(int argc, const char* const* argv): m_argc(argc), m_argv(argv) {
        if (argc < 0 || (argc > 0 && !argv)) {
            throw std::invalid_argument("ArgumentParser: invalid argc/argv");
        }
    }

    /** @brief Bind a reference; repeated matches replace its value in argument order. */
    template <StringConstant Name, typename T>
    ArgumentParser& Bind(T& target) {
        m_bindings.emplace_back([&target](StringView argument, const char*, bool positional) {
            StringView value;
            if (!detail::ParserArgumentMatcher<Name>::Match(argument, positional, value)) return 0;
            target = ParserDeserializer<T>{}.deserialize(value);
            return 1;
        });
        return *this;
    }

    /** @brief Bind a container; append each deserialized match without clearing it. */
    template <StringConstant Name, typename T, typename Container = Vector<T>>
    ArgumentParser& BindMulti(Container& target) {
        m_bindings.emplace_back([&target](StringView argument, const char*, bool positional) {
            StringView value;
            if (!detail::ParserArgumentMatcher<Name>::Match(argument, positional, value)) return 0;
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
            return 1;
        });
        return *this;
    }

    /** @brief Bind --name to a boolean; set it to true when the switch is present. */
    template <StringConstant Name>
    ArgumentParser& BindSwitch(bool& target) {
        return BindOptionSwitch<Name, false>(target);
    }

    /** @brief Bind -n to a boolean; set it to true when the switch is present. */
    template <StringConstant Name>
    ArgumentParser& BindShortSwitch(bool& target) {
        return BindOptionSwitch<Name, true>(target);
    }

    /** @brief Bind -n value to a reference; repeated matches replace its value. */
    template <StringConstant Name, typename T>
    ArgumentParser& BindShort(T& target) {
        return BindShortValue<Name>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind -n value to a container; append each deserialized value. */
    template <StringConstant Name, typename T, typename Container = Vector<T>>
    ArgumentParser& BindShortMulti(Container& target) {
        return BindShortValue<Name>([&target](StringView value) {
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
        });
    }

    /**
     * @brief Parse using the first matching option, then positional binding.
     * @throws std::invalid_argument For an unbound argument, missing short option
     *         value, or invalid value.
     * @note Conversion exceptions propagate. Earlier assignments remain on failure;
     *       calling Parse again reapplies assignments and appends multi-values again.
     *       Short values consume the next argument even if it begins with '-'.
     */
    void Parse() const {
        bool positionalOnly = false;
        for (int i = 1; i < m_argc;) {
            const StringView argument(m_argv[i]);
            if (!positionalOnly && argument == "--") {
                positionalOnly = true;
                ++i;
                continue;
            }
            const char* next = i + 1 < m_argc ? m_argv[i + 1] : nullptr;
            int consumed = positionalOnly ? 0 : ApplyBinding(argument, next, false);
            if (consumed == 0 && (positionalOnly || !argument.starts_with("--"))) {
                consumed = ApplyBinding(argument, next, true);
            }
            if (consumed == 0) throw std::invalid_argument("ArgumentParser: unbound argument");
            i += consumed;
        }
    }

private:
    template <StringConstant Name, bool Short>
    ArgumentParser& BindOptionSwitch(bool& target) {
        m_bindings.emplace_back([&target](StringView argument, const char*, bool positional) {
            if (positional || !detail::ParserOptionMatcher<Name, Short>::Match(argument)) return 0;
            target = true;
            return 1;
        });
        return *this;
    }

    template <StringConstant Name, typename Assign>
    ArgumentParser& BindShortValue(Assign assign) {
        m_bindings.emplace_back([assign = std::move(assign)](StringView argument, const char* next, bool positional) {
            if (positional || !detail::ParserOptionMatcher<Name, true>::Match(argument)) return 0;
            if (!next) throw std::invalid_argument("ArgumentParser: missing short option value");
            assign(StringView(next));
            return 2;
        });
        return *this;
    }

    int ApplyBinding(StringView argument, const char* next, bool positional) const {
        for (const auto& binding: m_bindings) {
            if (const int consumed = binding(argument, next, positional)) return consumed;
        }
        return 0;
    }

    int m_argc;
    const char* const* m_argv;
    Vector<std::function<int(StringView, const char*, bool)>> m_bindings;
};

LCORE_NAMESPACE_END
