#include "lcore/logger.hpp"
#include <ctime>
#include <chrono>
#include <map>
#include <fmt/color.h>

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::log;

static Level defaultLevel = Level::None;
static std::map<std::string_view, SimpleLogger*>* loggerMap;

USE_LOGGER("Compiler.Utils.Log");

LogTime log::GetLogTime()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_tm = std::localtime(&now_c);
    int milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    return LogTime{
        now_tm->tm_year + 1900,
        now_tm->tm_mon + 1,
        now_tm->tm_mday,
        now_tm->tm_hour,
        now_tm->tm_min,
        now_tm->tm_sec,
        milliseconds};
}

LogTime log::GetLogTime(long microsecond)
{
    int milliseconds = microsecond % 1000;
    time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(std::chrono::microseconds(microsecond)));
    auto now_tm = std::localtime(&time);

    return LogTime{
        now_tm->tm_year + 1900,
        now_tm->tm_mon + 1,
        now_tm->tm_mday,
        now_tm->tm_hour,
        now_tm->tm_min,
        now_tm->tm_sec,
        milliseconds};
}

LogTime log::GetLogTime(int year, int month, int day, int hour, int minute, int second, int millisecond)
{
    return LogTime{year, month, day, hour, minute, second, millisecond};
}

String SimpleFormatter::Format(const Message &msg)
{
    static std::map<Level, std::string> levelMap = {
        {Level::None, "None"},
        {Level::Debug, "Debug"},
        {Level::Info, "Info"},
        {Level::Warning, "Warning"},
        {Level::Error, "Error"},
        {Level::Fatal, "Fatal"},
    };

    static std::map<Level, fmt::color> colorMap = {
        {Level::None, fmt::color::gray},
        {Level::Debug, fmt::color::green},
        {Level::Info, fmt::color::white},
        {Level::Warning, fmt::color::yellow},
        {Level::Error, fmt::color::red},
        {Level::Fatal, fmt::color::red},
    };

    Level equLevel = (Level)(((int)msg.level / 10) * 10);

    std::stringstream ss;
    // yyyy-mm-dd hh:mm:ss.mmm [LEVEL][DECLARATION] LINE:FUNCTION:MESSAGE
    // ss << std::setfill('0') << std::setw(4) << msg.time.year << '-' <<
    //     std::setfill('0') << std::setw(2) << msg.time.month << '-' <<
    //     std::setfill('0') << std::setw(2) << msg.time.day << ' ' <<
    //     std::setfill('0') << std::setw(2) << msg.time.hour << ':' <<
    //     std::setfill('0') << std::setw(2) << msg.time.minute << ':' <<
    //     std::setfill('0') << std::setw(2) << msg.time.second << '.' <<
    //     std::setfill('0') << std::setw(3) << msg.time.millisecond;
    
    // ss << ' ' << '[' << Color::foreground(colorMap[equLevel])  << levelMap[equLevel] << Color::reset() << ']' << '[' <<
    //     msg.declear << "] " << msg.line << ':' << msg.function << ':' << msg.message;

    // const std::string leftBracket = foreground(Foreground::BRIGHT_BLACK) + "[" + reset();
    // const std::string rightBracket = foreground(Foreground::BRIGHT_BLACK) + "]" + reset();
    // const std::string colon = foreground(Foreground::BRIGHT_BLACK) + ":" + reset();

    // ss << leftBracket << control(Controls::BOLD) << msg.declear << reset() << rightBracket;
    // ss << leftBracket << foreground(colorMap[equLevel]) << levelMap[equLevel] << reset() << rightBracket;
    // ss << ' ' << foreground(Foreground::BRIGHT_BLACK) << std::setw(3) << msg.line << reset() << colon << 
    //         foreground(Foreground::BRIGHT_BLACK) << msg.function << colon << reset();
    // ss << msg.message << reset();

    ss << fmt::format(fmt::fg(fmt::color::gray), "[") << fmt::format(fmt::fg(colorMap[equLevel]) | fmt::emphasis::bold, "{}", levelMap[equLevel]) << fmt::format(fmt::fg(fmt::color::gray), "]");
    ss << fmt::format(fmt::fg(fmt::color::gray), "[") << fmt::format(fmt::fg(fmt::terminal_color::white), "{}", msg.declear) << fmt::format(fmt::fg(fmt::color::gray), "] ");
    ss << fmt::format(fmt::fg(fmt::color::gray), " :{0} :{1}:", msg.line, msg.function);
    ss << fmt::format(fmt::fg(fmt::terminal_color::white), "{}", msg.message);
    
    return ss.str();
}

SimpleLogger::SimpleLogger(std::string_view declare, std::string_view file): declare(declare), file(file){
    this->level = defaultLevel;
    static std::map<std::string_view, SimpleLogger*> _loggerMap;
    loggerMap = &_loggerMap;
    if (!_loggerMap.contains(declare))
    {
        _loggerMap.insert({declare, this});
    }else{
        ASSERT_FATAL(fmt::format("Logger for file {} already exists", file));
    }
}

void SimpleLogger::SetLevel(std::string_view declare, Level level){
    if (loggerMap->find(declare) != loggerMap->end())
    {
        (*loggerMap)[declare]->level = level;
    }else{
        ASSERT_FATAL(fmt::format("Logger for file {} not exists", declare));
    }
}

void SimpleLogger::SetAllLevel(Level level){
    for (auto &pair : *loggerMap)
    {
        pair.second->level = level;
    }
    defaultLevel = level;
}
