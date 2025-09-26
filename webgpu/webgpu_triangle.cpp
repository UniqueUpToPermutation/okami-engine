#include "webgpu_triangle.hpp"
#include "webgpu_utils.hpp"

#include "../transform.hpp"

#include <glog/logging.h>
#include <glm/glm.hpp>

using namespace okami;

Error WebgpuTriangleModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error WebgpuTriangleModule::StartupImpl(ModuleInterface&) {
    // We need the device to be set before we can create the pipeline
    if (!m_device) {
        return Error("WebGPU device not set - call SetDevice() first");
    }

    // First, create the transform bind group layout
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry = {};
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Vertex;
    bindGroupLayoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry.buffer.minBindingSize = sizeof(glm::mat4);
    
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindGroupLayoutEntry;
    
    m_transformBindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bindGroupLayoutDesc);
    
    // Create the transform uniform buffer
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.size = sizeof(glm::mat4);
    bufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    
    m_transformBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
    
    // Create the bind group
    WGPUBindGroupEntry bindGroupEntry = {};
    bindGroupEntry.binding = 0;
    bindGroupEntry.buffer = m_transformBuffer;
    bindGroupEntry.size = sizeof(glm::mat4);
    
    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = m_transformBindGroupLayout;
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &bindGroupEntry;
    
    m_transformBindGroup = wgpuDeviceCreateBindGroup(m_device, &bindGroupDesc);

    // Load shader from file
    std::string shaderSource = LoadShaderFile("triangle.wgsl");
    if (shaderSource.empty()) {
        return Error("Failed to load triangle.wgsl shader file");
    }
    
    // Create shader module from loaded WGSL code
    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = shaderSource.c_str();
    
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslDesc.chain;
    
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_device, &shaderDesc);
    
    // Create render pipeline
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = m_surfaceFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    WGPUMultisampleState multisampleState = {};
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;
    
    // Create pipeline layout with the transform bind group layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_transformBindGroupLayout;
    
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &pipelineLayoutDesc);
    
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.multisample = multisampleState;
    
    m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
    
    // Clean up pipeline layout and shader module
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(shaderModule);
    
    if (!m_pipeline) {
        return Error("Failed to create WebGPU triangle render pipeline");
    }
    
    LOG(INFO) << "WebGPU triangle module initialized successfully";
    return {};
}

void WebgpuTriangleModule::ShutdownImpl(ModuleInterface&) {
    if (m_transformBindGroup) {
        wgpuBindGroupRelease(m_transformBindGroup);
        m_transformBindGroup = nullptr;
    }
    
    if (m_transformBuffer) {
        wgpuBufferRelease(m_transformBuffer);
        m_transformBuffer = nullptr;
    }
    
    if (m_transformBindGroupLayout) {
        wgpuBindGroupLayoutRelease(m_transformBindGroupLayout);
        m_transformBindGroupLayout = nullptr;
    }
    
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
        // Get render pass encoder from the render pass info
        WGPURenderPassEncoder renderPass = info.m_info;

        // Use pre-calculated camera matrices from render pass info
        auto viewMat = info.m_viewMatrix;
        auto projMat = info.m_projMatrix;
        
        // Render triangles for each entity with DummyTriangleComponent
        m_storage->ForEach([this, time, transforms, renderPass, projMat, viewMat](entity_t e, DummyTriangleComponent const&) {
            auto modelMat = transforms->GetOr(e, Transform::Identity()).AsMatrix();
            
            // Calculate MVP matrix: Projection * View * Model
            auto mvpMatrix = projMat * viewMat * modelMat;
            
            // Update the transform uniform buffer with the MVP matrix
            wgpuQueueWriteBuffer(wgpuDeviceGetQueue(m_device), m_transformBuffer, 0, &mvpMatrix, sizeof(glm::mat4));
            
            // Set pipeline and bind group
            wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);
            wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_transformBindGroup, 0, nullptr);
            
            // Draw the triangle
            wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
        });
    }
    
    return {};
}void WebgpuTriangleModule::SetDevice(WGPUDevice device, WGPUTextureFormat surfaceFormat) {
    m_device = device;
    m_surfaceFormat = surfaceFormat;
}

WebgpuTriangleModule::WebgpuTriangleModule() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}