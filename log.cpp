#include "log.hpp"

#include <glog/logging.h>

using namespace okami;

void okami::LogInfo(std::string_view msg) {
    LOG(INFO) << msg;
}

void okami::LogWarning(std::string_view msg) {
    LOG(WARNING) << msg;
}

void okami::LogError(std::string_view msg) {
    LOG(ERROR) << msg;
}