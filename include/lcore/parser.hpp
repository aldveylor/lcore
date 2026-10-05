#pragma once
#include "string.hpp"
#include "exception.hpp"
#include "container/vector.hpp"
#include <charconv>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

LCORE_NAMESPACE_BEGIN

/** @brief Invalid argc/argv supplied to ArgvIterator. */
class ParserInvalidInputError final: public Exception {
public:
    /** @brief Return the fixed diagnostic for invalid iterator input. */
    const char* what() const noexcept override { return "ArgumentParser: invalid argc/argv"; }
};

/** @brief An argument has no matching registered binding. */
class ParserUnboundArgumentError final: public Exception {
public:
    /** @brief Return the fixed diagnostic for an unbound argument. */
    const char* what() const noexcept override { return "ArgumentParser: unbound argument"; }
};

/** @brief A required binding received no values. */
class ParserMissingArgumentError final: public Exception {
public:
    /** @brief Return the fixed diagnostic for a missing required argument. */
    const char* what() const noexcept override { return "ArgumentParser: required argument missing"; }
};

/** @brief A short option has no following value. */
class ParserMissingValueError final: public Exception {
public:
    /** @brief Return the fixed diagnostic for a missing option value. */
    const char* what() const noexcept override { return "ArgumentParser: missing option value"; }
};

/** @brief A value cannot be deserialized into its bound type. */
class ParserInvalidValueError final: public Exception {
public:
    /** @brief Return the fixed diagnostic for an invalid value. */
    const char* what() const noexcept override { return "ArgumentParser: invalid value"; }
};

/** @brief A numeric value exceeds the range of its bound type. */
class ParserValueOutOfRangeError final: public Exception {
public:
    /** @brief Return the fixed diagnostic for an out-of-range value. */
    const char* what() const noexcept override { return "ArgumentParser: value out of range"; }
};

/** @brief Forward iterator adapting argc/argv to borrowed StringView arguments. */
class ArgvIterator {
public:
    using value_type = StringView;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using iterator_concept = std::forward_iterator_tag;
    using reference = StringView;
    using pointer = void;

    constexpr ArgvIterator() noexcept = default;

    /** @brief Skip argv[0]; all argc entries must be non-null and outlive the iterator. */
    constexpr ArgvIterator(int argc, const char* const* argv) {
        if (argc < 0 || (argc > 0 && !argv)) {
            throw ParserInvalidInputError();
        }
        m_current = argc > 0 ? argv + 1 : argv;
        m_end = argc > 0 ? argv + argc : argv;
    }

    /** @brief Read the current argument; the iterator must not equal end(). */
    constexpr StringView operator*() const noexcept { return StringView(*m_current); }

    constexpr ArgvIterator& operator++() noexcept {
        if (m_current != m_end) ++m_current;
        return *this;
    }

    constexpr ArgvIterator operator++(int) noexcept {
        const auto previous = *this;
        ++*this;
        return previous;
    }

    constexpr bool operator==(const ArgvIterator& other) const noexcept {
        return m_current == other.m_current;
    }

    constexpr ArgvIterator end() const noexcept {
        auto result = *this;
        result.m_current = m_end;
        return result;
    }

private:
    const char* const* m_current = nullptr;
    const char* const* m_end = nullptr;
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
        if (value.empty()) throw ParserInvalidValueError();
        T result;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        if (parsed.ec == std::errc::result_out_of_range) {
            throw ParserValueOutOfRangeError();
        }
        if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
            throw ParserInvalidValueError();
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
        throw ParserInvalidValueError();
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
template <StringConstant Name>
struct ParserArgumentMatcher {
    static_assert(Name.size() > 0, "Option names must not be empty");

    static constexpr bool Match(StringView argument, StringView& value) noexcept {
        if (argument.size() < Name.size() + 3
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

}

/**
 * @brief Parse command-line arguments into bound references using template names.
 * @tparam Iterator Copyable iterator with value_type equal to StringView.
 * @note Arguments and bound targets must outlive parsing; targets
 *       storing StringView also borrow the argument strings. Named options use
 *       --name=value or -n value; switches use --name or -n. Registered options
 *       take priority over positional bindings, including for negative numbers.
 *       Positional Bind<T> and BindOptional<T> each consume at most one value,
 *       in registration order; BindMulti<T> consumes remaining positional values.
 *       A standalone -- disables option matching from that argument onward;
 *       -- itself and subsequent arguments are positional values, distributed
 *       according to positional binding capacities.
 *       Single-value Bind and BindShort require at least one value per Parse;
 *       optional, switch and multi bindings accept zero values.
 */
template <typename Iterator = ArgvIterator>
requires std::same_as<typename Iterator::value_type, StringView>
class ArgumentParser {
public:
    /** @brief Construct from the half-open argument range [begin, end). */
    ArgumentParser(Iterator begin, Iterator end): m_begin(std::move(begin)), m_end(std::move(end)) {}

    /** @brief Construct from an iterator that provides its own end iterator. */
    explicit ArgumentParser(Iterator begin)
    requires requires(Iterator iterator) { { iterator.end() } -> std::same_as<Iterator>; }
        : ArgumentParser(begin, begin.end()) {}

    /** @brief Bind exactly one positional argument to a reference. */
    template <typename T>
    ArgumentParser& Bind(T& target) {
        return BindPositionalValue<true>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind zero or more positional arguments; append without clearing the container. */
    template <typename T, typename Container = Vector<T>>
    ArgumentParser& BindMulti(Container& target) {
        return BindPositionalValue<false, true>([&target](StringView value) {
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
        });
    }

    /** @brief Bind a required named option; repeated matches replace the reference value. */
    template <StringConstant Name, typename T>
    requires (Name.size() > 0)
    ArgumentParser& Bind(T& target) {
        return BindValue<Name, true>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind zero or more named option values; append without clearing the container. */
    template <StringConstant Name, typename T, typename Container = Vector<T>>
    requires (Name.size() > 0)
    ArgumentParser& BindMulti(Container& target) {
        return BindValue<Name, false>([&target](StringView value) {
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
        });
    }

    /** @brief Bind at most one positional argument; preserve the reference when absent. */
    template <typename T>
    ArgumentParser& BindOptional(T& target) {
        return BindPositionalValue<false>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind an optional named option; preserve the reference when absent. */
    template <StringConstant Name, typename T>
    requires (Name.size() > 0)
    ArgumentParser& BindOptional(T& target) {
        return BindValue<Name, false>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
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

    /** @brief Bind a required -n value; repeated matches replace the reference value. */
    template <StringConstant Name, typename T>
    ArgumentParser& BindShort(T& target) {
        return BindShortValue<Name, true>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind zero or more -n values; append each deserialized value to the container. */
    template <StringConstant Name, typename T, typename Container = Vector<T>>
    ArgumentParser& BindShortMulti(Container& target) {
        return BindShortValue<Name, false>([&target](StringView value) {
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
        });
    }

    /** @brief Bind an optional -n value; preserve the reference when absent. */
    template <StringConstant Name, typename T>
    ArgumentParser& BindShortOptional(T& target) {
        return BindShortValue<Name, false>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /**
     * @brief Parse using the first matching option, then positional binding.
     * @throws ParserUnboundArgumentError For an unbound argument.
     * @throws ParserMissingValueError For a short option without a value.
     * @throws ParserMissingArgumentError If any required binding received no values.
     * @throws ParserInvalidValueError For an invalid built-in value.
     * @throws ParserValueOutOfRangeError For a built-in numeric overflow.
     * @note Conversion exceptions propagate. Earlier assignments remain on failure;
     *       calling Parse again reapplies assignments and appends multi-values again.
     *       Short values consume the next argument even if it begins with '-',
     *       except for the standalone -- positional marker.
     */
    void Parse() const {
        Vector<std::size_t> counts(m_bindings.size(), 0);
        auto current = m_begin;
        bool positionalOnly = false;
        while (current != m_end) {
            if (!positionalOnly && *current == "--") {
                positionalOnly = true;
            }
            bool matched = false;
            for (std::size_t i = positionalOnly ? m_optionBindings : 0; i < m_bindings.size(); ++i) {
                if (counts[i] >= m_bindings[i].maximum) continue;
                if (m_bindings[i].parse(current, positionalOnly)) {
                    ++counts[i];
                    matched = true;
                    break;
                }
            }
            if (!matched) throw ParserUnboundArgumentError();
        }
        for (std::size_t i = 0; i < m_bindings.size(); ++i) {
            if (counts[i] < m_bindings[i].minimum) throw ParserMissingArgumentError();
        }
    }

private:
    /** @brief Iterator callback with minimum and maximum numbers of accepted values. */
    struct Binding {
        std::function<bool(Iterator&, bool)> parse;
        std::size_t minimum;
        std::size_t maximum;
    };

    template <bool Positional, bool Required,
        std::size_t Maximum = std::numeric_limits<std::size_t>::max(), typename Callback>
    ArgumentParser& RegisterBinding(Callback callback) {
        Binding binding{std::move(callback), Required ? 1u : 0u, Maximum};
        if constexpr (Positional) {
            m_bindings.emplace_back(std::move(binding));
        } else {
            m_bindings.insert(m_bindings.begin() + m_optionBindings, std::move(binding));
            ++m_optionBindings;
        }
        return *this;
    }

    template <StringConstant Name, bool Required, typename Assign>
    ArgumentParser& BindValue(Assign assign) {
        return RegisterBinding<false, Required>([assign = std::move(assign)](Iterator& current, bool) {
            StringView value;
            if (!detail::ParserArgumentMatcher<Name>::Match(*current, value)) return false;
            assign(value);
            ++current;
            return true;
        });
    }

    template <bool Required, bool Multi = false, typename Assign>
    ArgumentParser& BindPositionalValue(Assign assign) {
        constexpr std::size_t maximum = Multi ? std::numeric_limits<std::size_t>::max() : 1;
        return RegisterBinding<true, Required, maximum>(
            [assign = std::move(assign)](Iterator& current, bool positionalOnly) {
            const StringView argument = *current;
            if (!positionalOnly && argument.starts_with("--")) return false;
            assign(argument);
            ++current;
            return true;
        });
    }

    template <StringConstant Name, bool Short>
    ArgumentParser& BindOptionSwitch(bool& target) {
        return RegisterBinding<false, false>([&target](Iterator& current, bool) {
            if (!detail::ParserOptionMatcher<Name, Short>::Match(*current)) return false;
            target = true;
            ++current;
            return true;
        });
    }

    template <StringConstant Name, bool Required, typename Assign>
    ArgumentParser& BindShortValue(Assign assign) {
        return RegisterBinding<false, Required>([assign = std::move(assign), end = m_end](Iterator& current, bool) {
            if (!detail::ParserOptionMatcher<Name, true>::Match(*current)) return false;
            auto value = current;
            ++value;
            if (value == end || *value == "--") throw ParserMissingValueError();
            assign(*value);
            ++value;
            current = std::move(value);
            return true;
        });
    }

    Iterator m_begin;
    Iterator m_end;
    Vector<Binding> m_bindings;
    std::size_t m_optionBindings = 0;
};

LCORE_NAMESPACE_END
