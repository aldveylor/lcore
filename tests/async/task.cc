#include <chrono>
#include <gtest/gtest.h>
#include "lcore/async/task.hpp"
#include "lcore/async/awaiter.hpp"
#include "timeout.hpp"

using namespace LCORE_NAMESPACE_NAME::async;

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(LazyTask, Execution) {
    int i = 0; // Reset global variable
    TriggedAwaitable awaitable1;
    TriggedAwaitable awaitable2;
    auto task1 = [&]() -> Lazy<void> {
        for (int j = 0; j < 3; ++j) {
            co_await awaitable1;
            i += 1;
        }
    };
    auto task2 = [&]() -> Lazy<void> {
        co_await [&]() -> Lazy<int> {
            for (int j = 0; j < 3; ++j) {
                co_await awaitable2;
                i += 2;
            }
            co_return i;
        }();
    };
    crash_after(std::chrono::seconds(5));

    auto t1 = task1();
    auto t2 = task2();

    t1.resume();
    t2.resume();
    
    for (int j = 0; j < 3; ++j) {
        awaitable1.wait_for_handle();
        awaitable1.trigger();
        awaitable2.wait_for_handle();
        awaitable2.trigger();
    }
    EXPECT_EQ(i, 9); // After 3 iterations, i should be 9 (0 + 1 + 2 + 1 + 2 + 1 + 2)
    EXPECT_TRUE(t1.done());
    EXPECT_TRUE(t2.done());
}

TEST(EagerTask, ImmediateExecution) {
    int i = 0; // Reset global variable
    TriggedAwaitable awaitable;
    auto task = [&]() -> Eager<void> {
        i = 1; // Set i to 1 immediately upon task creation
        for (int j = 0; j < 3; ++j) {
            co_await awaitable;
            i += 1;
        }
    };
    crash_after(std::chrono::seconds(5));
    auto t = task();

    EXPECT_EQ(i, 1); // i should be 1 immediately after task creation
    
    for (int j = 0; j < 3; ++j) {
        awaitable.wait_for_handle();
        awaitable.trigger();
    }
    EXPECT_EQ(i, 4); // After 3 iterations, i should be 3 (0 + 1 + 1 + 1)
    EXPECT_TRUE(t.done());
}
