#include <lcore/string.hpp>
#include <gtest/gtest.h>
#include <array>
#include <limits>

using namespace lcore;

TEST(StringTest, Slices) {
    const String str("abcdef");
    const StringView view(str);
    EXPECT_EQ(view.substr(2, 100), "cdef");
    EXPECT_EQ(str.substr(2, 100), "cdef");
    EXPECT_EQ(view.trim(1, 4), "bcd");
    EXPECT_EQ(str.trim(1, 4), "bcd");
    EXPECT_EQ(view.center(1, 2), "bcd");
    EXPECT_EQ(str.center(1, 2), "bcd");
    EXPECT_EQ(view.trim(1, 4).data(), str.data() + 1);
    EXPECT_TRUE(view.substr(6).empty());
    EXPECT_TRUE(view.trim(3, 3).empty());
    EXPECT_TRUE(view.center(3, 3).empty());
    EXPECT_TRUE(StringView().substr(0).empty());
    EXPECT_THROW(view.substr(7), std::out_of_range);
    EXPECT_THROW(str.substr(7), std::out_of_range);
    EXPECT_THROW(view.trim(4, 1), std::out_of_range);
    EXPECT_THROW(str.trim(0, 7), std::out_of_range);
    EXPECT_THROW(view.center(4, 3), std::out_of_range);
    EXPECT_THROW(str.center(7, 0), std::out_of_range);
    EXPECT_THROW(view.center(1, std::numeric_limits<size_t>::max()), std::out_of_range);
}

TEST(StringTest, Join) {
    const std::array<int, 3> values{1, 2, 3};
    const StringView sep(",");
    const String ownedSep(",");
    EXPECT_EQ(sep.Join(values), "1,2,3");
    EXPECT_EQ(ownedSep.Join(values), "1,2,3");
    EXPECT_EQ(StringView::Join(values, ""), "123");
    EXPECT_EQ(String::Join(values, "::"), "1::2::3");
    EXPECT_EQ(sep.Join(std::array<int, 0>{}), "");
    EXPECT_EQ(sep.Join(std::array<int, 1>{42}), "42");
    struct Range {
        const int* first;
        const int* last;
        const int* begin() const { return first; }
        const int* end() const { return last; }
    };
    EXPECT_EQ(sep.Join(Range{values.data(), values.data() + values.size()}), "1,2,3");
}

TEST(StringTest, ConversionsAndDigits) {
    const std::string_view standard("123");
    const StringView view(standard);
    const String str(view);
    EXPECT_EQ(view + "4", "1234");
    EXPECT_EQ(str + StringView("4"), "1234");
    EXPECT_TRUE(view.isdigit());
    EXPECT_TRUE(str.isdigit());
    EXPECT_TRUE(StringView().isdigit());
    EXPECT_FALSE(StringView("12a").isdigit());
    EXPECT_FALSE(String(char(0xff)).isdigit());
    EXPECT_TRUE(String(StringView()).empty());
    const char bytes[] = {'a', '\0', 'b'};
    EXPECT_EQ(String(StringView(bytes, 3)).size(), 3u);
}

TEST(CStringViewTest, TerminationAndConversions) {
    const char bytes[] = {'a', 'b', 'c'};
    CStringView owned(StringView(bytes, 3));
    EXPECT_STREQ(owned.c_str(), "abc");
    EXPECT_EQ(static_cast<StringView>(owned), "abc");
    EXPECT_EQ(static_cast<String>(owned), "abc");
    CStringView borrowed("hello");
    EXPECT_EQ(static_cast<String>(borrowed), "hello");
    CStringView empty{StringView()};
    EXPECT_STREQ(empty.c_str(), "");
    EXPECT_STREQ(CStringView(nullptr).c_str(), "");
    const char embedded[] = {'a', '\0', 'b'};
    CStringView binary(StringView(embedded, 3));
    EXPECT_EQ(static_cast<StringView>(binary).size(), 3u);
    EXPECT_EQ(static_cast<String>(binary), String(embedded, 3));
    EXPECT_EQ(binary.c_str()[3], '\0');
}

TEST(CStringViewTest, CopyAndMoveLifetime) {
    for (const auto& value : {String("short"), String(128, 'x')}) {
        CStringView copy("initial");
        CStringView moved("initial");
        {
            CStringView original(value);
            copy = original;
            CStringView intermediate(original);
            moved = std::move(intermediate);
        }
        EXPECT_EQ(static_cast<StringView>(copy), value);
        EXPECT_EQ(static_cast<StringView>(moved), value);
        CStringView constructed(std::move(copy));
        EXPECT_EQ(static_cast<String>(constructed), value);
        EXPECT_STREQ(constructed.c_str(), value.c_str());
    }
    CStringView temporary(String("temporary"));
    EXPECT_EQ(static_cast<StringView>(temporary), "temporary");
}
