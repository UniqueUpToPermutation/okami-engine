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

// WebGPU resource release functions
void okami::WgpuRelease(WGPUDevice ptr) {
    if (ptr) wgpuDeviceRelease(ptr);
}

void okami::WgpuRelease(WGPUBuffer ptr) {
    if (ptr) wgpuBufferRelease(ptr);
}

void okami::WgpuRelease(WGPUTexture ptr) {
    if (ptr) wgpuTextureRelease(ptr);
}

void okami::WgpuRelease(WGPUTextureView ptr) {
    if (ptr) wgpuTextureViewRelease(ptr);
}

void okami::WgpuRelease(WGPUSampler ptr) {
    if (ptr) wgpuSamplerRelease(ptr);
}

void okami::WgpuRelease(WGPUBindGroup ptr) {
    if (ptr) wgpuBindGroupRelease(ptr);
}

void okami::WgpuRelease(WGPUBindGroupLayout ptr) {
    if (ptr) wgpuBindGroupLayoutRelease(ptr);
}

void okami::WgpuRelease(WGPUPipelineLayout ptr) {
    if (ptr) wgpuPipelineLayoutRelease(ptr);
}

void okami::WgpuRelease(WGPURenderPipeline ptr) {
    if (ptr) wgpuRenderPipelineRelease(ptr);
}

void okami::WgpuRelease(WGPUComputePipeline ptr) {
    if (ptr) wgpuComputePipelineRelease(ptr);
}

void okami::WgpuRelease(WGPUShaderModule ptr) {
    if (ptr) wgpuShaderModuleRelease(ptr);
}

void okami::WgpuRelease(WGPUCommandEncoder ptr) {
    if (ptr) wgpuCommandEncoderRelease(ptr);
}

void okami::WgpuRelease(WGPUCommandBuffer ptr) {
    if (ptr) wgpuCommandBufferRelease(ptr);
}

void okami::WgpuRelease(WGPURenderPassEncoder ptr) {
    if (ptr) wgpuRenderPassEncoderRelease(ptr);
}

void okami::WgpuRelease(WGPUComputePassEncoder ptr) {
    if (ptr) wgpuComputePassEncoderRelease(ptr);
}

void okami::WgpuRelease(WGPUQuerySet ptr) {
    if (ptr) wgpuQuerySetRelease(ptr);
}

void okami::WgpuRelease(WGPUSurface ptr) {
    if (ptr) wgpuSurfaceRelease(ptr);
}

void okami::WgpuRelease(WGPUAdapter ptr) {
    if (ptr) wgpuAdapterRelease(ptr);
}

void okami::WgpuRelease(WGPUInstance ptr) {
    if (ptr) wgpuInstanceRelease(ptr);
}

void okami::WgpuRelease(WGPUQueue ptr) {
    if (ptr) wgpuQueueRelease(ptr);
}