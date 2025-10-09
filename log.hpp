#pragma once

#include <string_view>
#include <string>

namespace okami {
    void LogInfo(std::string_view msg);
    void LogWarning(std::string_view msg);
    void LogError(std::string_view msg);
    void LogInfo(std::string const& msg);
    void LogWarning(std::string const& msg);
    void LogError(std::string const& msg);
}