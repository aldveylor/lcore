/**
 * @file logger.hpp
 * @author liyanes (liyanes@outlook.com)
 * @brief Logger utils, used for debuging and logging
 * @version 0.1
 * @date 2024-06-04
 * 
 * @copyright Copyright (c) 2024
 * 
 * @note This header should not be included in other header files, to avoid macro pollution
 */
#pragma once
#include "base.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "string.hpp"

#ifdef LCORE_DEBUG
#define ENABLE_LOGDEBUG
#endif

LCORE_NAMESPACE_BEGIN

namespace log {

struct LogTime{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millisecond;
};

LogTime GetLogTime();
LogTime GetLogTime(long microsecond);
LogTime GetLogTime(int year, int month, int day, int hour, int minute, int second, int millisecond);

enum class Level: int{
    None = 0,
    Debug = 10,
    Info = 20,
    Warning = 30,
    Error = 40,
    Fatal = 50,
};

/// @brief The log message struct
struct Message{
    /// @brief The log message
    std::string message;
    /// @brief The declearation of the log message (usually Object name used for logging)
    std::string declear;
    /// @brief The filename of the source code
    std::string file;
    /// @brief The line number of the source code
    int line;
    /// @brief The function name of the source code
    std::string function;
    /// @brief The log level of the message
    Level level;
    /// @brief The time of the log message
    LogTime time;
};

/// @brief The log formatter interface
/// @tparam MsgStruct The message struct type
template <typename MsgStruct = Message>
requires std::is_base_of<Message, MsgStruct>::value
class Formatter{
public:
    /// @brief Format the log message
    /// @param msg The log message
    /// @return The formatted log message
    virtual String Format(const MsgStruct& msg) = 0;
    inline virtual ~Formatter() = default;
};

/// @brief Logger class
/// @tparam MsgStruct The message struct type
template <typename MsgStruct = Message>
class Logger{
protected:
    virtual void _Log(const MsgStruct& msg) = 0;
    Formatter<MsgStruct>* formatter;
public:
    Level level = Level::None;
    inline bool IsEnabled(Level level) const {
        return static_cast<int>(level) >= static_cast<int>(this->level);
    }
    inline void Log(const MsgStruct& msg) {
        if (IsEnabled(msg.level)){
            _Log(msg);
        }
    }
    inline virtual ~Logger() = default;
};

class SimpleFormatter: public Formatter<Message>{
public:
    String Format(const Message& msg) override;
};

/// @brief Stream logger class, used to log message to output stream
/// @tparam MsgStruct The message struct type
/// @tparam OutStream The output stream type
template <typename MsgStruct = Message, typename OutStream = std::ostream>
requires std::is_base_of<Message, MsgStruct>::value && std::is_base_of<std::ostream, OutStream>::value
class StreamLogger: public Logger<MsgStruct>{
    OutStream& outStream;
public:
    /// @brief Construct a new Stream Logger object
    /// @param outStream The output stream
    StreamLogger(OutStream& outStream = std::cerr): outStream(outStream){
        this->formatter = new SimpleFormatter();
    }
    ~StreamLogger(){
        delete this->formatter;
    }
protected:
    /// @brief Log the message
    /// @param msg The log message
    void _Log(const MsgStruct& msg) override{
        outStream << this->formatter->Format(msg) << std::endl;
    }
};

class SimpleLogger: public StreamLogger<Message>{
    std::string declare;
    std::string file;
public:
    SimpleLogger(std::string_view declare, std::string_view file);

    void Debug(std::string message, int line, std::string function){
        Log(Message{
            message,
            declare,
            file,
            line,
            function,
            Level::Debug,
            GetLogTime()
        });
    }

    void Info(std::string message, int line, std::string function){
        Log(Message{
            message,
            declare,
            file,
            line,
            function,
            Level::Info,
            GetLogTime()
        });
    }

    void Warning(std::string message, int line, std::string function){
        Log(Message{
            message,
            declare,
            file,
            line,
            function,
            Level::Warning,
            GetLogTime()
        });
    }

    void Error(std::string message, int line, std::string function){
        Log(Message{
            message,
            declare,
            file,
            line,
            function,
            Level::Error,
            GetLogTime()
        });
    }

    void Fatal(std::string message, int line, std::string function){
        Log(Message{
            message,
            declare,
            file,
            line,
            function,
            Level::Fatal,
            GetLogTime()
        });
    }

    /// @brief Set the log level of specific logger
    /// @param declare The declare name of the logger
    /// @param level The log level
    static void SetLevel(std::string_view declare, Level level);
    /// @brief Set the log level of all loggers
    /// @param level The log level
    static void SetAllLevel(Level level);
};

}

LCORE_NAMESPACE_END

/// Macro function below should only be used in the source file (.cc or .cpp)
#define USE_LOGGER(declare) static LCORE_NAMESPACE::log::SimpleLogger logger(declare, __FILE__);
#define LOG_DEBUG(message) do {if (logger.IsEnabled(LCORE_NAMESPACE::log::Level::Debug)) logger.Debug(message, __LINE__, __FUNCTION__);} while (0)
#define LOG_INFO(message) do {if (logger.IsEnabled(LCORE_NAMESPACE::log::Level::Info)) logger.Debug(message, __LINE__, __FUNCTION__);} while (0)
#define LOG_WARNING(message) do {if (logger.IsEnabled(LCORE_NAMESPACE::log::Level::Warning)) logger.Debug(message, __LINE__, __FUNCTION__);} while (0)
#define LOG_ERROR(message) do {if (logger.IsEnabled(LCORE_NAMESPACE::log::Level::Error)) logger.Debug(message, __LINE__, __FUNCTION__);} while (0)
#define LOG_FATAL(message) do {if (logger.IsEnabled(LCORE_NAMESPACE::log::Level::Fatal)) logger.Debug(message, __LINE__, __FUNCTION__);} while (0)

#define LOG_DEBUGFORMAT(...) LOG_DEBUG(std::format(__VA_ARGS__))
#define LOG_INFOFORMAT(...) LOG_INFO(std::format(__VA_ARGS__))
#define LOG_WARNINGFORMAT(...) LOG_WARNING(std::format(__VA_ARGS__))
#define LOG_ERRORFORMAT(...) LOG_ERROR(std::format(__VA_ARGS__))
#define LOG_FATALFORMAT(...) LOG_FATAL(std::format(__VA_ARGS__))

#ifndef ENABLE_LOGDEBUG
#undef LOG_DEBUG
#define LOG_DEBUG(message) do {} while(0)
#endif

#define DEFINE_LOG_LEVEL(declare, level) do {LCORE_NAMESPACE::log::SimpleLogger::SetLevel(declare, level); } while(0)
#define DEFINE_ALL_LOG_LEVEL(level) do {LCORE_NAMESPACE::log::SimpleLogger::SetAllLevel(level); } while (0)
#define ENABLE_LOG_DEBUG(declare) DEFINE_LOG_LEVEL(declare, LCORE_NAMESPACE::log::Level::Debug)

/**

Usage example:

In the source file (.cc or .cpp):
```cpp
#include "logger.hpp"
USE_LOGGER("MySourceFile")

int main() {


    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARNING("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_FATAL("This is a fatal message");

    int x = 42;
    LOG_PARAMETERS(x);
    LOG_FUNCTION(x);

    ASSERT_FATAL_IF(x != 42, "x should be 42");

    return 0;
}
```

 */
