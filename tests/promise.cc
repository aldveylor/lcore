#include <gtest/gtest.h>
#include "lcore/promise.hpp"
#include "timeout.hpp"

using namespace LCORE_NAMESPACE;

using namespace std::chrono_literals;

TEST(Promise, Basic) {
    Promise<int> promise;
    auto future = promise.GetFuture();
    bool called = false;

    future.Then([&](int value) {
        called = true;
        EXPECT_EQ(value, 42);
    });
    promise.Complete(42);

    EXPECT_TRUE(called);
}

TEST(Promise, ThreadCall) {
    auto _ = crash_after(5s);
    Promise<int> promise;
    auto future = promise.GetFuture();
    bool called = false;

    future.Then([&](int value) {
        called = true;
        EXPECT_EQ(value, 42);
    });

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        promise.Complete(42);
    });

    future.Wait();
    t.join();

    EXPECT_TRUE(called);
}
