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
 *       --name=value. Other arguments are positional, including negative numbers.
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
        m_bindings.emplace_back([&target](StringView argument, bool positional) {
            StringView value;
            if (!detail::ParserArgumentMatcher<Name>::Match(argument, positional, value)) return false;
            target = ParserDeserializer<T>{}.deserialize(value);
            return true;
        });
        return *this;
    }

    /** @brief Bind a container; append each deserialized match without clearing it. */
    template <StringConstant Name, typename T, typename Container = Vector<T>>
    ArgumentParser& BindMulti(Container& target) {
        m_bindings.emplace_back([&target](StringView argument, bool positional) {
            StringView value;
            if (!detail::ParserArgumentMatcher<Name>::Match(argument, positional, value)) return false;
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
            return true;
        });
        return *this;
    }

    /**
     * @brief Parse into the first registered binding matching each argument.
     * @throws std::invalid_argument For an unbound argument or invalid value.
     * @note Conversion exceptions propagate. Earlier assignments remain on failure;
     *       calling Parse again reapplies assignments and appends multi-values again.
     */
    void Parse() const {
        bool positionalOnly = false;
        for (int i = 1; i < m_argc; ++i) {
            const StringView argument(m_argv[i]);
            if (!positionalOnly && argument == "--") {
                positionalOnly = true;
                continue;
            }
            const bool positional = positionalOnly || !argument.starts_with("--");
            bool matched = false;
            for (const auto& binding: m_bindings) {
                if (binding(argument, positional)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) throw std::invalid_argument("ArgumentParser: unbound argument");
        }
    }

private:
    int m_argc;
    const char* const* m_argv;
    Vector<std::function<bool(StringView, bool)>> m_bindings;
};

LCORE_NAMESPACE_END
