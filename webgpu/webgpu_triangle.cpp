#include "webgpu_triangle.hpp"

#include "../transform.hpp"

#include <glog/logging.h>

using namespace okami;

Error WebgpuTriangleModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error WebgpuTriangleModule::StartupImpl(ModuleInterface&) {
    // We need the device to be set before we can create the pipeline
    if (!m_device) {
        return Error("WebGPU device not set - call SetDevice() first");
    }

    // Create triangle render pipeline with WGSL shaders
    const char* vertexShaderSource = R"(
        @vertex fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4<f32> {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>( 0.0,  0.5),
                vec2<f32>(-0.5, -0.5),
                vec2<f32>( 0.5, -0.5)
            );
            return vec4<f32>(pos[vertexIndex], 0.0, 1.0);
        }
    )";
    
    const char* fragmentShaderSource = R"(
        @fragment fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0, 0.0, 0.0, 1.0);
        }
    )";
    
    // Create vertex shader
    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = vertexShaderSource;
    
    WGPUShaderModuleDescriptor vertexShaderDesc = {};
    vertexShaderDesc.nextInChain = &wgslDesc.chain;
    
    WGPUShaderModule vertexShader = wgpuDeviceCreateShaderModule(m_device, &vertexShaderDesc);
    
    // Create fragment shader
    wgslDesc.code = fragmentShaderSource;
    WGPUShaderModuleDescriptor fragmentShaderDesc = {};
    fragmentShaderDesc.nextInChain = &wgslDesc.chain;
    
    WGPUShaderModule fragmentShader = wgpuDeviceCreateShaderModule(m_device, &fragmentShaderDesc);
    
    // Create render pipeline
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = m_surfaceFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    WGPUFragmentState fragmentState = {};
    fragmentState.module = fragmentShader;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    WGPUMultisampleState multisampleState = {};
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;
    
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.vertex.module = vertexShader;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.multisample = multisampleState;
    
    m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
    
    // Clean up shader modules
    wgpuShaderModuleRelease(vertexShader);
    wgpuShaderModuleRelease(fragmentShader);
    
    if (!m_pipeline) {
        return Error("Failed to create WebGPU triangle render pipeline");
    }
    
    LOG(INFO) << "WebGPU triangle module initialized successfully";
    return {};
}

void WebgpuTriangleModule::ShutdownImpl(ModuleInterface&) {
    if (m_pipeline) {
        wgpuRenderPipelineRelease(m_pipeline);
        m_pipeline = nullptr;
    }
}

Error WebgpuTriangleModule::ProcessFrameImpl(Time const&, ModuleInterface& mi) {
    return {};
}

Error WebgpuTriangleModule::MergeImpl() {
    return {};
}
    
std::string_view WebgpuTriangleModule::GetName() const {
    return "WebGPU Triangle Module";
}

Error WebgpuTriangleModule::Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) {
    
    auto transforms = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!transforms) {
        return Error("No IComponentView<Transform> available in WebGPUTriangleModule");
    }
    
    if (!m_storage->IsEmpty() && m_pipeline) {
        // Get render pass encoder from the user data
        WGPURenderPassEncoder renderPass = info.m_info;

        // Render triangles for each entity with DummyTriangleComponent
        m_storage->ForEach([this, time, transforms, renderPass](entity_t e, DummyTriangleComponent const&) {
            auto transform = transforms->GetOr(e, Transform::Identity()).AsMatrix();
            
            // Note: WebGPU doesn't have built-in transform matrices like bgfx
            // For now, we'll just render at identity. Later we could add uniform buffers
            // for transform matrices.
            
            wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);
            wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
        });
    }
    
    return {};
}

void WebgpuTriangleModule::SetDevice(WGPUDevice device, WGPUTextureFormat surfaceFormat) {
    m_device = device;
    m_surfaceFormat = surfaceFormat;
}

WebgpuTriangleModule::WebgpuTriangleModule() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}