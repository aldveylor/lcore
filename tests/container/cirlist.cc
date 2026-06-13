#include <gtest/gtest.h>
#include <lcore/container/circulelist.hpp>
#include <iostream>

using namespace LCORE_NAMESPACE;

TEST(CircularListTest, BasicOperations) {
    CircularList<int> list;
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);

    EXPECT_EQ(list.size(), 3);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.back(), 3);

    auto iter = list.begin();
    EXPECT_EQ(*iter, 1);
    ++iter;
    EXPECT_EQ(*iter, 2);
    ++iter;
    EXPECT_EQ(*iter, 3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
