#pragma once
#include "config.h"
#include <string>
#include <string_view>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <utility>
#include "traits.hpp"

LCORE_NAMESPACE_BEGIN

class String;
class StringStream;

class StringView: public std::string_view {
public:
    using std::string_view::basic_string_view;
    inline StringView(const std::string& string);
    inline StringView(std::string_view view);
    inline StringView(const char* str);

    inline String operator+(StringView rhs) const;
    inline StringView substr(size_t pos, size_t n = npos) const;
    inline StringView trim(size_t lpos, size_t rpos) const;
    inline StringView center(size_t lslice, size_t rslice) const;
    inline bool isdigit() const noexcept;

    template <Iterable Container>
    inline String Join(Container&& container) const;

    template <Iterable Container>
    inline static String Join(Container&& container, StringView sep);
};

class String: public std::string {
public:
    using std::string::basic_string;
    inline String(const StringView view);
    inline String(std::string&& str);
    inline String(char ch);
    
    inline String& operator=(const std::string& str);
    inline String& operator=(const StringView view);
    inline String& operator=(const char* str);

    inline String operator+(StringView view) const;
    inline StringView substr(size_t pos, size_t n = npos) const;
    inline StringView trim(size_t lpos, size_t rpos) const;
    inline StringView center(size_t lslice, size_t rslice) const;
    inline bool isdigit() const noexcept;

    template <Iterable Container>
    inline String Join(Container&& container) const;

    template <Iterable Container>
    inline static String Join(Container&& container, StringView sep);
};

class StringStream: public std::stringstream {
public:
    String str() const {
        return std::stringstream::str();
    };
    void str(const std::string& str) {
        return std::stringstream::str(str);
    };
};

/// @brief A null-terminated string adapter compatible with StringView and String.
/// @note String views and strings are copied; C strings are borrowed and must outlive the adapter.
class CStringView {
public:
    inline CStringView(StringView view): m_cache(view) {}
    inline CStringView(const String& str): m_cache(str) {}
    inline CStringView(const char* cstr): m_cstr(cstr ? cstr : "") {}

    inline operator const char*() const noexcept { return c_str(); }
    inline operator StringView() const noexcept {
        return m_cstr ? StringView(m_cstr) : StringView(m_cache);
    }
    inline operator String() const { return String(static_cast<StringView>(*this)); }

    inline const char* c_str() const & noexcept{
        return m_cstr ? m_cstr : m_cache.c_str();
    }
private:
    String m_cache;
    // A non-null pointer always refers to a borrowed C string, never to m_cache.
    const char* m_cstr = nullptr;
};

inline StringView::StringView(const std::string& string): std::string_view(string.data(), string.size()) {}
inline StringView::StringView(std::string_view view): std::string_view(view) {}
inline StringView::StringView(const char* str): std::string_view(str) {}

inline String StringView::operator+(StringView rhs) const{
    StringStream ss;
    ss << *this << rhs;
    return ss.str();
};

inline StringView StringView::substr(size_t pos, size_t n) const {
    return std::string_view::substr(pos, n);
};
// Select the half-open interval [lpos, rpos).
inline StringView StringView::trim(size_t lpos, size_t rpos) const {
    if (lpos > rpos || rpos > size()) throw std::out_of_range("StringView::trim");
    return substr(lpos, rpos - lpos);
};
// Remove lslice characters from the left and rslice characters from the right.
inline StringView StringView::center(size_t lslice, size_t rslice) const {
    if (lslice > size() || rslice > size() - lslice) throw std::out_of_range("StringView::center");
    return substr(lslice, size() - lslice - rslice);
};
template <Iterable Container>
inline String StringView::Join(Container&& container) const {
    return Join(std::forward<Container>(container), *this);
};
template <Iterable Container>
inline String StringView::Join(Container&& container, StringView sep) {
    StringStream ss;
    auto iter = container.begin();
    const auto end = container.end();
    if (iter == end) return "";
    ss << *iter;
    while (++iter != end) ss << sep << *iter;
    return ss.str();
};

inline bool StringView::isdigit() const noexcept {
    for (auto c: *this){
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
};

inline String::String(const StringView view): std::string(static_cast<std::string_view>(view)) {}
inline String::String(std::string&& str): std::string(std::move(str)) {}
inline String::String(char ch): std::string(1, ch) {}

inline String& String::operator=(const std::string& str){
    std::string::operator=(str);
    return *this;
};
inline String& String::operator=(const StringView view){
    std::string::operator=(view);
    return *this;
};
inline String& String::operator=(const char* str){
    std::string::operator=(str);
    return *this;
};

inline String String::operator+(StringView view) const{
    StringStream ss;
    ss << *this << view;
    return ss.str();
};
inline StringView String::substr(size_t pos, size_t n) const {
    return StringView(*this).substr(pos, n);
};
inline StringView String::trim(size_t lpos, size_t rpos) const {
    return StringView(*this).trim(lpos, rpos);
};
inline StringView String::center(size_t lslice, size_t rslice) const {
    return StringView(*this).center(lslice, rslice);
};
inline bool String::isdigit() const noexcept {
    return StringView(*this).isdigit();
};
template <Iterable Container>
inline String String::Join(Container&& container) const {
    return Join(std::forward<Container>(container), *this);
};
template <Iterable Container>
inline String String::Join(Container&& container, StringView sep) {
    return StringView::Join(std::forward<Container>(container), sep);
};

LCORE_NAMESPACE_END

namespace std {

template<>
struct hash<LCORE_NAMESPACE::String> {
    size_t operator()(const LCORE_NAMESPACE::String& str) const noexcept {
        return std::hash<std::string>()(str);
    }
};

template<>
struct hash<LCORE_NAMESPACE::StringView> {
    size_t operator()(const LCORE_NAMESPACE::StringView& view) const noexcept {
        return std::hash<std::string_view>()(view);
    }
};

}
