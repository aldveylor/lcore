/**
 * @file timeout.hpp
 * @brief A simple timeout utility for testing asynchronous code.
 */
#pragma once
#include <chrono>
#include <thread>
#include <memory>
#include <functional>
#include <iostream>

inline std::function<void()> crash_after(std::chrono::milliseconds duration) {
    std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);
    std::thread([duration, cancelled]() {
        auto left = duration;
        while (left.count() > 0) {
            if (*cancelled) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            left -= std::chrono::milliseconds(10);
        }
        std::cerr << "Test timed out after " << duration.count() << " milliseconds." << std::endl;
        std::terminate();
    }).detach();
    return [cancelled]() {
        *cancelled = true;
    };
}
