#include <chrono>
#include <gtest/gtest.h>
#include "lcore/async/sharedtask.hpp"
#include "lcore/async/awaiter.hpp"
#include "timeout.hpp"

using namespace LCORE_NAMESPACE::async;

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(LazySharedTask, Execution) {
    int i = 0; // Reset global variable
    TriggedAwaitable awaitable1;
    TriggedAwaitable awaitable2;
    auto task1 = [&]() -> SharedLazy<void> {
        i += 1;
        for (int j = 0; j < 3; ++j) {
            co_await awaitable1;
            i += 1;
        }
    };
    auto t1 = task1();

    auto task2 = [&]() -> SharedLazy<void> {
        i += 2;
        co_await t1;                            // Await the completion of task1 (lvalue reference)
                                                // Just wait for task1 to complete without taking over its execution
        co_await [&]() -> SharedLazy<int> {     // Rvalue Shared Task with only single reference
                                                // Take over the execution of task2 until completion
            for (int j = 0; j < 3; ++j) {
                co_await awaitable2;
                i += 2;
            }
            co_return i;
        }();
        std::cout << "Task2 completed, i = " << i << std::endl;
    };
    auto t2 = task2();

    auto _ = crash_after(std::chrono::seconds(5));

    EXPECT_EQ(i, 0); // i should be 0 immediately after task creation

    t1.resume();
    t2.resume();

    EXPECT_EQ(i, 3) ; // After resuming, i should be 3 (0 + 1 + 2)

    for (int j = 0; j < 3; ++j) {
        awaitable1.wait_for_handle();
        awaitable1.trigger();
    }
    for (int j = 0; j < 3; ++j) {
        awaitable2.wait_for_handle();
        awaitable2.trigger();
    }
    EXPECT_EQ(i, 12); // After all iterations, i should be 12 (3 + 1 + 1 + 1 + 2 + 2 + 2)
    EXPECT_TRUE(t1.done());
    EXPECT_TRUE(t2.done());
}

TEST(EagerSharedTask, ImmediateExecution) {
    int i = 0; // Reset global variable
    TriggedAwaitable awaitable;
    auto task = [&]() -> SharedEager<void> {
        i = 1; // Set i to 1 immediately upon task creation
        for (int j = 0; j < 3; ++j) {
            co_await awaitable;
            i += 1;
        }
    };
    auto _  = crash_after(std::chrono::seconds(5));
    auto t = task();

    EXPECT_EQ(i, 1); // i should be 1 immediately after task creation
    
    for (int j = 0; j < 3; ++j) {
        awaitable.wait_for_handle();
        awaitable.trigger();
    }
    EXPECT_EQ(i, 4); // After 3 iterations, i should be 4 (1 + 1 + 1 + 1)
    EXPECT_TRUE(t.done());
}
