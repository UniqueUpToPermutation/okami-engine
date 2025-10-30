#include "log.hpp"

#include <glog/logging.h>

using namespace okami;

void okami::LogInfo(std::string_view msg, std::string_view file, int line) {
    LOG(INFO) << msg << " (" << file << ":" << line << ")";
}

void okami::LogWarning(std::string_view msg, std::string_view file, int line) {
    LOG(WARNING) << msg << " (" << file << ":" << line << ")";
}

void okami::LogError(std::string_view msg, std::string_view file, int line) {
    LOG(ERROR) << msg << " (" << file << ":" << line << ")";
}

void okami::LogInfo(std::string const& msg, std::string const& file, int line) {
    LOG(INFO) << msg << " (" << file << ":" << line << ")";
}

void okami::LogWarning(std::string const& msg, std::string const& file, int line) {
    LOG(WARNING) << msg << " (" << file << ":" << line << ")";
}

void okami::LogError(std::string const& msg, std::string const& file, int line) {
    LOG(ERROR) << msg << " (" << file << ":" << line << ")";
}