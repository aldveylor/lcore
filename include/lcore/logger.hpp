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

namespace _detail {

/// @brief The repr function, used to convert the object to string
/// @tparam ...Args The type of the object
/// @param ...args The object
/// @return The string representation of the object
/// @note This function is used for logging purpose
/// @note User code should not use this function directly, use repr() if needed
template <typename ...Args>
inline std::vector<std::string> __LogRepr(Args&&... args){
    return {repr(args)...};
}

/// @brief Join a list of names and parameters into a string
/// @param names The parameter names
/// @param args The parameter values
/// @return The joined string
template <size_t N>
inline std::string __LogJoin(std::array<std::string_view, N>&& names, std::vector<std::string>&& args){
    std::stringstream ss;
    if (names.size() == 1 && names[0].empty()) return "";
    for (size_t i = 0; i < names.size(); i++){
        ss << names[i] << "=" << args[i] << ", ";
    }
    std::string ret = ss.str();
    return ret.substr(0, ret.size() - 2);
}

template <size_t N>
inline constexpr std::array<std::string_view, N> __LogSplit(const char* value){
    size_t len = 0;
    while (value[len] != '\0') len++;
    if (len == 0) return {};
    else {
        std::array<std::string_view, N> ret;
        size_t bracket = 0;
        size_t pos = 0;
        size_t index = 0;
        for (size_t i = 0; i < len; i++){
            if (value[i] == '(' || value[i] == '[' || value[i] == '{'){
                bracket++;
            }else if (value[i] == ')' || value[i] == ']' || value[i] == '}'){
                bracket--;
            }
            if (value[i] == ',' && bracket == 0){
                ret[index++] = std::string_view(value + pos, i - pos);
                pos = i + 1;
            }
        }
        ret[index] = std::string_view(value + pos, len - pos);
        return ret;
    }
}

}

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

#define _LOGSTREAM_LOG(message, method, level) do                   \
{                                                                   \
    if (!logger.IsEnabled(level)) break;                            \
    std::stringstream ss;                                           \
    ss << message;                                                  \
    method(ss.str(), __LINE__, __FUNCTION__);                       \
} while(0)

#ifdef ENABLE_LOGDEBUG
#define LOGSTREAM_DEBUG(message) _LOGSTREAM_LOG(message, logger.Debug, LCORE_NAMESPACE::log::Level::Debug)
#else
#define LOGSTREAM_DEBUG(message) do {} while(0)
#endif
#define LOGSTREAM_INFO(message) _LOGSTREAM_LOG(message, logger.Info, LCORE_NAMESPACE::log::Level::Info)
#define LOGSTREAM_WARNING(message) _LOGSTREAM_LOG(message, logger.Warning, LCORE_NAMESPACE::log::Level::Warning)
#define LOGSTREAM_ERROR(message) _LOGSTREAM_LOG(message, logger.Error, LCORE_NAMESPACE::log::Level::Error)
#define LOGSTREAM_FATAL(message) _LOGSTREAM_LOG(message, logger.Fatal, LCORE_NAMESPACE::log::Level::Fatal)

#define LOG_PARAMETERS(...) \
    LOGSTREAM_DEBUG("(" << LCORE_NAMESPACE::_detail::__LogJoin<GET_ARG_COUNT(__VA_ARGS__)>( \
        LCORE_NAMESPACE::_detail::__LogSplit<GET_ARG_COUNT(__VA_ARGS__)>(#__VA_ARGS__), \
        LCORE_NAMESPACE::_detail::__LogRepr(__VA_ARGS__)) << ")")

#define LOG_FUNCTION(...) \
    LOGSTREAM_DEBUG(__FUNCTION__ << "(" << LCORE_NAMESPACE::_detail::__LogJoin<GET_ARG_COUNT(__VA_ARGS__)>( \
        LCORE_NAMESPACE::_detail::__LogSplit<GET_ARG_COUNT(__VA_ARGS__)>(#__VA_ARGS__), \
        LCORE_NAMESPACE::_detail::__LogRepr(__VA_ARGS__)) << ")")

#define DEFINE_LOG_LEVEL(declare, level) do {LCORE_NAMESPACE::log::SimpleLogger::SetLevel(declare, level); } while(0)
#define DEFINE_ALL_LOG_LEVEL(level) do {LCORE_NAMESPACE::log::SimpleLogger::SetAllLevel(level); } while (0)
#define ENABLE_LOG_DEBUG(declare) DEFINE_LOG_LEVEL(declare, LCORE_NAMESPACE::log::Level::Debug)

/**
 * Debug related macro
*/

#define ASSERT_FATAL(message) do {                              \
    LOGSTREAM_FATAL(message);                                   \
    __asm__ __volatile__ ("int $3");                            \
} while(0)

#define ASSERT_FATAL_IF(condition, message) do {                         \
    if (!(condition)){                                           \
        LOGSTREAM_FATAL("Failed to assert: " << #condition << " => " << message);     \
        __asm__ __volatile__("int $3");                         \
    }                                                           \
} while(0)

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