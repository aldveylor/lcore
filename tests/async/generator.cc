#include <gtest/gtest.h>
#include "lcore/async/generator.hpp"
#include "lcore/container.hpp"

using namespace LCORE_NAMESPACE_NAME::async;

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(GeneratorTest, CopyableTest) {
    auto yiledtest = [] () -> Generator<int> {
        co_yield 1;
        co_yield 2;
        co_yield 3;
    };

    std::vector<int> results;
    for (auto i : yiledtest()) {
        results.push_back(i);
    }
    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    EXPECT_EQ(results[2], 3);
}

TEST(GeneratorTest, ReferenceTest) {
    auto yiledtest = [] () -> Generator<std::shared_ptr<int>> {
        co_yield std::make_shared<int>(1);
        co_yield std::make_shared<int>(2);
        co_yield std::make_shared<int>(3);
    };
    std::vector<std::shared_ptr<int>> results;
    for (auto i : yiledtest()) {
        results.push_back(i);
    }
    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(*results[0], 1);
    EXPECT_EQ(*results[1], 2);
    EXPECT_EQ(*results[2], 3);
}

TEST(GeneratorTest, ViewTest) {
    auto viewtest = [] () -> Generator<int> {
        int i = 0;
        while (true) {
            co_yield i++;
        }
    };
    std::vector<int> results;
    for (auto i : viewtest() | std::views::take(10)) {
        results.push_back(i);
    }
    ASSERT_EQ(results.size(), 10);
    for (int j = 0; j < 10; ++j) {
        EXPECT_EQ(results[j], j);
    }
}

TEST(ProductTest, BasicFunctionality) {
    std::vector<int> vec = {1, 2, 3};
    std::list<int> lst = {4, 5};
    std::vector<double> dvec = {1.1, 2.2};

    auto pd = product(vec, lst, dvec);
    std::vector<std::tuple<int, int, double>> results;

    for (auto [v, l, d] : pd) {
        results.emplace_back(v, l, d);
    }

    ASSERT_EQ(results.size(), 12);
    EXPECT_EQ(results[0], std::make_tuple(1, 4, 1.1));
    EXPECT_EQ(results[1], std::make_tuple(1, 4, 2.2));
    EXPECT_EQ(results[2], std::make_tuple(1, 5, 1.1));
    EXPECT_EQ(results[3], std::make_tuple(1, 5, 2.2));
    EXPECT_EQ(results[4], std::make_tuple(2, 4, 1.1));
    EXPECT_EQ(results[5], std::make_tuple(2, 4, 2.2));
}

TEST(ProductTest, EmptyContainer) {
    std::vector<int> vec = {};
    std::list<int> lst = {4, 5};

    auto pd = product(vec, lst);
    std::vector<std::tuple<int, int>> results;

    for (auto [v, l] : pd) {
        results.emplace_back(v, l);
    }

    ASSERT_TRUE(results.empty());
}
