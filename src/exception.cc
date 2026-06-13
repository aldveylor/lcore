#include "lcore/exception.hpp"
#include "lcore/config.h"
#include <execinfo.h>
#include <iostream>

using namespace LCORE_NAMESPACE;

std::vector<const char*> LCORE_NAMESPACE::GetStacktrace(int skip, size_t size){
    void *buffer[LCORE_STACKTRACE_SIZE];
    size_t stacktrace_size = backtrace(buffer, LCORE_STACKTRACE_SIZE);
    char **stacktrace = backtrace_symbols(buffer, stacktrace_size);
    if (stacktrace == nullptr){
        return {};
    }
    std::vector<const char*> result;
    for (auto ptr = stacktrace + skip; ptr < stacktrace + stacktrace_size && result.size() < size; ++ptr){
        if (*ptr == nullptr){
            break;
        }
        result.push_back(*ptr);
    }
    free(stacktrace);
    return result;
}

#ifdef LCORE_ENABLE_RECORDSTACK
Exception::Exception(): m_stacktrace(GetStacktrace(1)) {}

void Exception::PrintBacktrace() const {
    for (const auto& line: m_stacktrace){
        std::cerr << line << std::endl;
    }
}

std::vector<const char*> Exception::GetBackTrace() const& {
    return m_stacktrace;
}
#endif
