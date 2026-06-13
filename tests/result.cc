#include <gtest/gtest.h>
#include "lcore/result.hpp"

using namespace LCORE_NAMESPACE;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(ResultTest, BasicUsage) {
    Result<int, std::string> res1(42);
    EXPECT_TRUE(res1.IsOk());
    EXPECT_FALSE(res1.IsError());
    EXPECT_EQ(res1.Value(), 42);

    Result<int, std::string> res2(Error<std::string>("An error occurred"));
    EXPECT_FALSE(res2.IsOk());
    EXPECT_TRUE(res2.IsError());
    EXPECT_EQ(res2.Error(), "An error occurred");

    EXPECT_THROW(res1.Error(), RuntimeError);
    EXPECT_THROW(res2.Value(), RuntimeError);
}

TEST(ResultTest, MonadicOperations) {
    Result<int, std::string> res1(10);
    auto res2 = res1.Transform([](int v) { return v * 2; });
    EXPECT_TRUE(res2.IsOk());
    EXPECT_EQ(res2.Value(), 20);

    auto res3 = res2.AndThen([](int v) { return "Success"; });
    EXPECT_TRUE(res3.IsOk());
    EXPECT_EQ(res3.Value(), "Success");

    Result<int, std::string> errRes(Error<std::string>("Initial error"));
    auto errRes2 = errRes.OrElse([](const std::string& err) { return std::string(err + " - Handled"); });
    EXPECT_FALSE(errRes2.IsOk());
    EXPECT_EQ(errRes2.Error(), "Initial error - Handled");

    auto errRes3 = errRes2.TransformError([](const std::string& err) { return err + " - Transformed"; });
    EXPECT_FALSE(errRes3.IsOk());
    EXPECT_EQ(errRes3.Error(), "Initial error - Handled - Transformed");
}
