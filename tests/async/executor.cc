#include <gtest/gtest.h>
#include "lcore/async/executor.hpp"
#include "timeout.hpp"
#include <chrono>

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::async;

using namespace std::chrono_literals;

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Duration = SteadyClock::duration;

TEST(Scheduler, Run)
{
    auto _ = crash_after(1000ms);
    Scheduler& scheduler = Scheduler::GetInstance();
    bool executed1 = false;
    bool executed2 = false;
    scheduler.Schedule([&]()->Lazy<void> {
        executed1 = true;
        co_return;
    }());
    scheduler.Schedule([&]()->SharedLazy<void> {
        co_await [&]()->Lazy<void> {
            executed2 = true;
            co_return;
        }();
        co_return;
    }());
    scheduler.Run();
    EXPECT_TRUE(executed1);
    EXPECT_TRUE(executed2);
}

TEST(TimeComponent, Sleep)
{
    auto _ = crash_after(1000ms);
    Scheduler& scheduler = Scheduler::GetInstance();
    
    Duration elapsed = Duration::zero();
    scheduler.Schedule([&]()->Lazy<void> {
        TimePoint start = SteadyClock::now();
        co_await Sleep(100ms);
        TimePoint end = SteadyClock::now();
        elapsed = end - start;
    }());

    scheduler.Run();

    EXPECT_GE(elapsed, 100ms);
}

TEST(TimeComponent, SleepUntil)
{
    auto _ = crash_after(1000ms);
    Scheduler& scheduler = Scheduler::GetInstance();

    Duration elapsed = Duration::zero();
    scheduler.Schedule([&]()->Lazy<void> {
        TimePoint start = SteadyClock::now();
        co_await SleepUntil(start + 100ms);
        TimePoint end = SteadyClock::now();
        elapsed = end - start;
    }());

    scheduler.Run();
    EXPECT_GE(elapsed, 100ms);
}

TEST(TimeComponent, SetTimeout)
{
    auto _ = crash_after(1000ms);
    Scheduler& scheduler = Scheduler::GetInstance();

    bool called = false;
    Duration elapsed = Duration::zero();

    scheduler.Schedule([&]()->Lazy<void> {
        TimePoint start = SteadyClock::now();
        SetTimeout(100ms, [&] {
            elapsed = SteadyClock::now() - start;
            called = true;
        });
        co_await Sleep(200ms);
    }());

    scheduler.Run();

    EXPECT_TRUE(called);
    EXPECT_GE(elapsed, 100ms);
}

TEST(TimeComponent, SetTimeoutCancel)
{
    auto _ = crash_after(1000ms);
    Scheduler& scheduler = Scheduler::GetInstance();

    bool called = false;
    bool called2 = false;

    scheduler.Schedule([&]()->Lazy<void> {
        auto cancel = SetTimeout(100ms, [&] {
            called = true;
        });
        SetTimeout(150ms, [&] {
            called2 = true;
        });
        cancel();
        co_await Sleep(200ms);
    }());

    scheduler.Run();
    EXPECT_FALSE(called);
    EXPECT_TRUE(called2);
}

TEST(TimeComponent, SetInterval)
{
    auto _ = crash_after(2000ms);
    Scheduler& scheduler = Scheduler::GetInstance();

    int count = 0;

    scheduler.Schedule([&]()->Lazy<void> {
        auto cancel = SetInterval(50ms, [&] {
            count++;
        });
        co_await Sleep(220ms);
        cancel();
    }());

    scheduler.Run();
    EXPECT_GE(count, 3);
    EXPECT_LE(count, 6);
}

