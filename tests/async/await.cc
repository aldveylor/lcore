#include <gtest/gtest.h>
#include <lcore/async/task.hpp>
#include <lcore/async/awaiter.hpp>
#include <type_traits>
#include "lcore/base.hpp"
#include "timeout.hpp"

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::async;

using namespace std::chrono_literals;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(AwaiterTest, CallbackAwaiterTest) {
    auto _ = crash_after(5s); // Set a timeout to prevent hanging
    
    auto task = []() -> Lazy<int> {
        co_return co_await MakeCallbackAwaiter([](std::function<void(int)> callback) {
            callback(42);
        });
    }();
    task.resume(); // Should be finished
    EXPECT_TRUE(task.done());
    EXPECT_EQ(std::move(task).consume_value(), 42);

    class ExampleClass {
    public:
        ExampleClass() { 
            std::cout << "ExampleClass constructor called" << std::endl;
        }
        ExampleClass(const ExampleClass&) = delete; // Disable copy constructor
        ExampleClass(ExampleClass&&) noexcept {}
        ~ExampleClass() { 
            std::cout << "ExampleClass destructor called" << std::endl;
        }
    };
    auto task2 = []() -> Lazy<void> {
        co_await MakeCallbackAwaiter([](std::function<void(ExampleClass)> callback) {
            std::thread([callback = std::move(callback)]() mutable {
                callback(ExampleClass{});   // Callback with a temporary ExampleClass object, construction happens here
                                            // The coroutine will be executed in this thread
            }).detach();
        }); // Destruction of the temporary ExampleClass happens here
        // Continue in the detached thread
        co_return;
    }();
    std::stringstream output;
    std::streambuf* oldCoutBuffer = std::cout.rdbuf(output.rdbuf());
    
    // crash_after(std::chrono::seconds(5)); // Timeout has been set
    try {
        task2.resume(); // Should be finished
    } catch (const std::exception& e) {
        std::cout.rdbuf(oldCoutBuffer);
        FAIL() << "Exception thrown: " << e.what();
    } catch (...) {
        std::cout.rdbuf(oldCoutBuffer);
        FAIL() << "Unknown exception thrown";
    }
    while (!task2.done()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Wait for the task to complete
    }
    std::cout.rdbuf(oldCoutBuffer);
    auto str = output.str();
    EXPECT_NE(str.find("ExampleClass constructor called"), std::string::npos);
    EXPECT_NE(str.find("ExampleClass destructor called"), std::string::npos);
    EXPECT_EQ(str.find("ExampleClass constructor called"), str.rfind("ExampleClass constructor called"));
}

TEST(AwaiterTest, WhenAllTest) {
    auto _ = crash_after(5s);

    Scheduler& scheduler = Scheduler::GetInstance();
    auto start = std::chrono::steady_clock::now();
    bool done1 = false, done2 = false;
    auto whenall = [&]()-> Lazy<void> {
        auto res = co_await WhenAll([&]() -> Lazy<void> {
            co_await Sleep(100ms);
            done1 = true;
        }(), [&]() -> Lazy<int> {
            co_await Sleep(150ms);
            done2 = true;
            co_return 42;
        }());
        EXPECT_TRUE(done1);
        EXPECT_TRUE(done2);
        EXPECT_EQ(std::get<0>(res), Monostate{});
        EXPECT_EQ(std::get<1>(res), 42);

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        EXPECT_GE(duration.count(), 150); // Total duration should be at least 150ms
        EXPECT_LT(duration.count(), 200); // Total duration should be less than 200ms (parrel execution)
    }();

    scheduler.Schedule(std::move(whenall));
    scheduler.Run();
}

TEST(AwaiterTest, WhenAnyTest) {
    auto _ = crash_after(5s);

    Scheduler& scheduler = Scheduler::GetInstance();
    bool done1 = false, done2 = false;
    auto start = std::chrono::steady_clock::now();
    auto whenany = [&]()-> Lazy<void> {
        auto [i, res] = co_await WhenAny([&]() -> Lazy<void> {
            co_await Sleep(100ms);
            done1 = true;
        }(), [&]() -> Lazy<void> {
            co_await Sleep(150ms);
            done2 = true;
        }());
        EXPECT_TRUE(done1 || done2);
        EXPECT_FALSE(done1 && done2);

        EXPECT_EQ(i, 0); // The first task should complete first
        EXPECT_TRUE(std::get<0>(res).has_value());
        EXPECT_FALSE(std::get<1>(res).has_value());
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        EXPECT_GE(duration.count(), 100); // Total duration should be at least 100ms
        EXPECT_LT(duration.count(), 150); // Total duration should be less than 150ms
    }();
    scheduler.Schedule(std::move(whenany));
    scheduler.Run();
}

TEST(AwaiterTest, WhenAllWithExceptionTest) {
    auto _ = crash_after(5s);

    Scheduler& scheduler = Scheduler::GetInstance();
    auto whenall = [&]()-> Lazy<void> {
        bool done1 = false, done2 = false;
        try {
            auto res = co_await WhenAll([&]() -> Lazy<void> {
                co_await Sleep(100ms);
                throw RuntimeError("Task 1 failed");
                done1 = true;
            }(), [&]() -> Lazy<int> {
                co_await Sleep(150ms);
                co_return 42;
                done2 = true;
            }());
            throw RuntimeError("WhenAll should have thrown an exception");
        } catch (const std::exception& e) {
            EXPECT_STREQ(e.what(), "Task 1 failed");
        }
        EXPECT_FALSE(done1); // Task 1 should not have completed
        EXPECT_FALSE(done2); // Task 2 should not have completed (because WhenAll should have thrown an exception immediately when Task 1 failed)
    }();
    scheduler.Schedule(std::move(whenall));
    scheduler.Run();
}
