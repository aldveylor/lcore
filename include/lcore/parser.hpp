#pragma once
#include "string.hpp"
#include <cstddef>
#include <stdexcept>
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

/**
 * @brief Non-owning command-line cursor with template string comparisons.
 * @note Skips argv[0]. The argument array and strings must outlive the parser
 *       and all returned views. Unknown options and positional arguments are
 *       handled by the caller through argument() and Next().
 */
class ArgumentParser {
public:
    /** @brief Construct from argc/argv; each of the argc entries must be non-null. */
    constexpr ArgumentParser(int argc, const char* const* argv):
        m_argc(argc), m_argv(argv), m_index(argc > 0 ? 1 : 0) {
        if (argc < 0 || (argc > 0 && !argv)) {
            throw std::invalid_argument("ArgumentParser: invalid argc/argv");
        }
    }

    constexpr bool hasNext() const noexcept { return m_index < m_argc; }

    /** @brief Current argument, or an empty view when the cursor is exhausted. */
    constexpr StringView argument() const noexcept {
        return hasNext() ? StringView(m_argv[m_index]) : StringView();
    }

    /** @brief Advance by one argument; return whether another argument remains. */
    constexpr bool Next() noexcept {
        if (hasNext()) ++m_index;
        return hasNext();
    }

    /** @brief Compare the current argument exactly with any template string. */
    template <StringConstant... Names>
    requires (sizeof...(Names) > 0)
    constexpr bool matches() const noexcept {
        return hasNext() && (Matches<Names>(argument()) || ...);
    }

    /** @brief Consume a matching flag; leave the cursor unchanged on mismatch. */
    template <StringConstant... Names>
    requires (sizeof...(Names) > 0)
    constexpr bool Consume() noexcept {
        if (!matches<Names...>()) return false;
        Next();
        return true;
    }

    /**
     * @brief Consume a named option in either --name=value or --name value form.
     * @param value Receives a borrowed value view, including an explicitly empty value.
     * @return False on mismatch, leaving both the cursor and value unchanged.
     * @throws std::invalid_argument If a matching option has no following value.
     * @note The separate value may start with '-'. Short option bundles are not expanded.
     */
    template <StringConstant... Names>
    requires (sizeof...(Names) > 0)
    constexpr bool ConsumeValue(StringView& value) {
        if (!hasNext()) return false;
        const auto current = argument();
        const auto separator = current.find('=');
        const auto name = current.substr(0, separator);
        if (!(Matches<Names>(name) || ...)) return false;

        if (separator != StringView::npos) {
            value = current.substr(separator + 1);
            Next();
        } else {
            if (m_index + 1 >= m_argc) {
                throw std::invalid_argument("ArgumentParser: missing option value");
            }
            value = StringView(m_argv[m_index + 1]);
            m_index += 2;
        }
        return true;
    }

private:
    template <StringConstant Name, std::size_t... Indices>
    static constexpr bool MatchesCharacters([[maybe_unused]] StringView value, std::index_sequence<Indices...>) noexcept {
        return ((value[Indices] == Name.data[Indices]) && ...);
    }

    template <StringConstant Name>
    static constexpr bool Matches(StringView value) noexcept {
        return value.size() == Name.size()
            && MatchesCharacters<Name>(value, std::make_index_sequence<Name.size()>{});
    }

    int m_argc;
    const char* const* m_argv;
    int m_index;
};

LCORE_NAMESPACE_END
