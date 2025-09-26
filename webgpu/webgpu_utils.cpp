#include "webgpu_utils.hpp"

#include "../paths.hpp"

#include <glog/logging.h>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace okami;

std::string okami::LoadShaderFile(const std::string& filename) {
    std::filesystem::path shaderPath = GetWebGPUShaderPath(filename);
    
    if (std::filesystem::exists(shaderPath)) {
        std::ifstream file(shaderPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            LOG(INFO) << "Loaded shader from: " << shaderPath.string();
            return buffer.str();
        } else {
            LOG(ERROR) << "Could not open shader file: " << shaderPath.string();
        }
    } else {
        LOG(ERROR) << "Could not find shader file: " << shaderPath.string();
    }
    
    return "";
}