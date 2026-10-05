#pragma once
#include "string.hpp"
#include "container/vector.hpp"
#include <charconv>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

LCORE_NAMESPACE_BEGIN

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
            throw std::invalid_argument("ArgvIterator: invalid argc/argv");
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
 *       Bind<T> and BindMulti<T> bind positional arguments and consume all
 *       arguments following a standalone --.
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

    /** @brief Bind positional arguments to a reference; repeated matches replace its value. */
    template <typename T>
    ArgumentParser& Bind(T& target) {
        return BindPositionalValue([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind positional arguments to a container; append without clearing it. */
    template <typename T, typename Container = Vector<T>>
    ArgumentParser& BindMulti(Container& target) {
        return BindPositionalValue([&target](StringView value) {
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
        });
    }

    /** @brief Bind a named option to a reference; repeated matches replace its value. */
    template <StringConstant Name, typename T>
    requires (Name.size() > 0)
    ArgumentParser& Bind(T& target) {
        return BindValue<Name>([&target](StringView value) {
            target = ParserDeserializer<T>{}.deserialize(value);
        });
    }

    /** @brief Bind a named option to a container; append without clearing it. */
    template <StringConstant Name, typename T, typename Container = Vector<T>>
    requires (Name.size() > 0)
    ArgumentParser& BindMulti(Container& target) {
        return BindValue<Name>([&target](StringView value) {
            target.push_back(ParserDeserializer<T>{}.deserialize(value));
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
        auto current = m_begin;
        while (current != m_end) {
            bool matched = false;
            for (const auto& binding: m_bindings) {
                if (binding(current)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) throw std::invalid_argument("ArgumentParser: unbound argument");
        }
    }

private:
    template <bool Positional, typename Binding>
    ArgumentParser& RegisterBinding(Binding binding) {
        if constexpr (Positional) {
            m_bindings.emplace_back(std::move(binding));
        } else {
            m_bindings.insert(m_bindings.begin() + m_optionBindings, std::move(binding));
            ++m_optionBindings;
        }
        return *this;
    }

    template <StringConstant Name, typename Assign>
    ArgumentParser& BindValue(Assign assign) {
        return RegisterBinding<false>([assign = std::move(assign)](Iterator& current) {
            StringView value;
            if (!detail::ParserArgumentMatcher<Name>::Match(*current, value)) return false;
            assign(value);
            ++current;
            return true;
        });
    }

    template <typename Assign>
    ArgumentParser& BindPositionalValue(Assign assign) {
        return RegisterBinding<true>([assign = std::move(assign), end = m_end](Iterator& current) {
            if (current == end) return false;
            const StringView argument = *current;
            if (argument == "--") {
                ++current;
                while (current != end) {
                    assign(*current);
                    ++current;
                }
                return true;
            }
            if (argument.starts_with("--")) return false;
            assign(argument);
            ++current;
            return true;
        });
    }

    template <StringConstant Name, bool Short>
    ArgumentParser& BindOptionSwitch(bool& target) {
        return RegisterBinding<false>([&target](Iterator& current) {
            if (!detail::ParserOptionMatcher<Name, Short>::Match(*current)) return false;
            target = true;
            ++current;
            return true;
        });
    }

    template <StringConstant Name, typename Assign>
    ArgumentParser& BindShortValue(Assign assign) {
        return RegisterBinding<false>([assign = std::move(assign), end = m_end](Iterator& current) {
            if (!detail::ParserOptionMatcher<Name, true>::Match(*current)) return false;
            auto value = current;
            ++value;
            if (value == end) throw std::invalid_argument("ArgumentParser: missing short option value");
            assign(*value);
            ++value;
            current = std::move(value);
            return true;
        });
    }

    Iterator m_begin;
    Iterator m_end;
    Vector<std::function<bool(Iterator&)>> m_bindings;
    std::size_t m_optionBindings = 0;
};

LCORE_NAMESPACE_END
