#pragma once

#include <string_view>
#include <string>

#define OKAMI_LOG_INFO(msg) \
    okami::LogInfo(msg, __FILE__, __LINE__)
#define OKAMI_LOG_WARNING(msg) \
    okami::LogWarning(msg, __FILE__, __LINE__)
#define OKAMI_LOG_ERROR(msg) \
    okami::LogError(msg, __FILE__, __LINE__)

namespace okami {
    void LogInfo(const char* msg, std::string_view file, int line);
    void LogWarning(const char* msg, std::string_view file, int line);
    void LogError(const char* msg, std::string_view file, int line);
    void LogInfo(std::string_view msg, std::string_view file, int line);
    void LogWarning(std::string_view msg, std::string_view file, int line);
    void LogError(std::string_view msg, std::string_view file, int line);
    void LogInfo(std::string const& msg, std::string_view file, int line);
    void LogWarning(std::string const& msg, std::string_view file, int line);
    void LogError(std::string const& msg, std::string_view file, int line);
}