#pragma once
#include "config.h"
#ifdef LCORE_DEBUG
#include <iostream>
#include <mutex>
#include <format>

#define LCORE_ABORT() do {              \
    __asm__ __volatile__("int $3");      \
} while (0)

#ifdef LCORE_ENABLE_ASSERT
#define LCORE_ASSERT(condition, msg)                                                            \
    do {                                                                                        \
        if (!bool(condition)){                                                                      \
            std::cerr << "Assertion failed: " << #condition << ", " << msg << std::endl;        \
            LCORE_ABORT();                                                                      \
        }                                                                                       \
    } while (0)
#endif // LCORE_ENABLE_ASSERT

#define LCORE_LOG(content) do { \
    std::cerr << "Log: " << __FILE__ << ":" << __LINE__ << ": " << __FUNCTION__ << ": " << content << std::endl; \
} while (0)


#else // not LCORE_DEBUG

#define LCORE_ABORT() do {std::abort();} while (0)
#define LCORE_LOG(content) do {} while (0)

#endif // LCORE_DEBUG

// Empty macro
#ifndef LCORE_ASSERT
#define LCORE_ASSERT(condition, msg) do {} while (0)
#endif

#define LCORE_ERROR(msg) LCORE_LOG("Error: " << msg)

#ifdef LCORE_ENABLE_ASSERT
#define LCORE_ASSERT_ERROR(condition, msg) do { \
    if (!bool(condition)) { \
        LCORE_ERROR(msg); \
    } \
} while (0)
#else
#define LCORE_ASSERT_ERROR(condition, msg) do {} while (0)
#endif

#define LCORE_FATAL(msg) do {LCORE_LOG("Fatal: " << msg); exit(-1);} while (0)
