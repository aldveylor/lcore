#include <gtest/gtest.h>
#include <lcore/async/task.hpp>
#include <lcore/async/awaiter.hpp>
#include <type_traits>
#include "timeout.hpp"

using namespace lcore;
using namespace lcore::async;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(AwaiterTest, CallbackAwaiterTest) {
    auto task = []() -> Lazy<int> {
        co_return co_await MakeCallbackAwaiter([](std::function<void(int)> callback) {
            callback(42);
        });
    }();
    crash_after(std::chrono::seconds(5)); // Set a timeout to prevent hanging
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
