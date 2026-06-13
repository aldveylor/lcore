#pragma once
#include <exception>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include "base.hpp"

#define LCORE_STACKTRACE_SIZE 64

LCORE_NAMESPACE_BEGIN

/// @brief Get the stacktrace
/// @param skip The number of stack frames to skip
/// @param size The number of stack frames to get, if the size is greater than LCORE_STACKTRACE_SIZE, the result will be truncated
/// @return The stacktrace
std::vector<const char*> GetStacktrace(int skip = 0, size_t size = LCORE_STACKTRACE_SIZE);

/// @brief The base class of all exceptions in lcore
class Exception: public std::exception {
#ifdef LCORE_ENABLE_RECORDSTACK
protected:
    /// @brief The stacktrace, if empty, the stacktrace is not recorded (or an error occurred while recording the stacktrace)
    std::vector<const char*> m_stacktrace;
public:
    /// @brief Construct an exception
    Exception();
    /// @brief Print the stacktrace
    void PrintBacktrace() const;
    /// @brief Get the stacktrace
    /// @return The stacktrace, the result may be empty
    std::vector<const char*> GetBackTrace() const&;
#else
public:
    Exception() = default;
#endif
};

/// @brief The runtime error
class RuntimeError: public Exception {
protected:
    const char* m_msg;
public:
    RuntimeError(const char* msg): m_msg(msg) {}
    inline const char* what() const noexcept override {
        return m_msg;
    }
};

/// @brief The not implemented error
class NotImplementedError final: public RuntimeError {
    std::string m_msg;
    const char* m_func;
    const char* m_file;
    int m_line;
public:
    NotImplementedError(const char* func, const char* file, int line): RuntimeError("Not implemented"), m_func(func), m_file(file), m_line(line) {}
    inline const char* what() const noexcept override {
        if (m_msg.empty()){
            std::stringstream oss;
            oss << "Function " << m_func << " in file " << m_file << " at line " << m_line << " is not implemented";
            const_cast<NotImplementedError*>(this)->m_msg = oss.str();
        }
        return m_msg.c_str();
    }
    inline const char* GetFunction() const noexcept {
        return m_func;
    }
    inline const char* GetFile() const noexcept {
        return m_file;
    }
    inline int GetLine() const noexcept {
        return m_line;
    }
};

/// @brief The system error (commonly errno)
class SystemError: public Exception {
protected:
    int m_errno;
public:
    SystemError(int errno_ = errno): m_errno(errno_) {}
    inline const char* what() const noexcept override {
        return strerror(m_errno);
    }
    inline int GetErrno() const noexcept {
        return m_errno;
    }
};

/// @brief The logic error
class LogicError: public Exception {
protected:
    const char* m_msg;
public:
    LogicError(const char* msg): m_msg(msg) {}
    inline const char* what() const noexcept override {
        return m_msg;
    }
};

LCORE_NAMESPACE_END

#define LCORE_NOTIMPLEMENTED() do {throw LCORE_NAMESPACE::NotImplementedError(__func__, __FILE__, __LINE__);} while(0)
