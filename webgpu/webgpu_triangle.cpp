#include "webgpu_triangle.hpp"
#include "webgpu_utils.hpp"

#include "../transform.hpp"

#include <glog/logging.h>
#include <glm/glm.hpp>
#include <vector>

using namespace okami;

Error WebgpuTriangleModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error WebgpuTriangleModule::StartupImpl(ModuleInterface& mi) {
    auto* renderer = mi.m_interfaces.Query<IWgpuRenderer>();
    if (!renderer) {
        return Error("IWgpuRenderer interface not available for WebGPU triangle module");
    }
    auto device = renderer->GetDevice();

    // First, create the transform bind group layout
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry = {};
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Vertex;
    bindGroupLayoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry.buffer.hasDynamicOffset = true;
    bindGroupLayoutEntry.buffer.minBindingSize = sizeof(glm::mat4);
    
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindGroupLayoutEntry;
    
    m_transformBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
    
    // Create the transform uniform buffer - large enough for multiple triangles
    uint32_t transformBufferSize = TRANSFORM_ALIGNMENT * MAX_TRIANGLES;
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.size = transformBufferSize;
    bufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    
    m_transformBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    
    // Create the bind group for dynamic offsets
    WGPUBindGroupEntry bindGroupEntry = {};
    bindGroupEntry.binding = 0;
    bindGroupEntry.buffer = m_transformBuffer;
    bindGroupEntry.size = sizeof(glm::mat4); // Size of a single transform
    
    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = m_transformBindGroupLayout;
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &bindGroupEntry;
    
    m_transformBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

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
    
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    
    // Create render pipeline
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = renderer->GetPreferredSwapchainFormat();
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
    
    // Configure depth-stencil state - always use depth
    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilBack.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;
    
    // Create pipeline layout with the transform bind group layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_transformBindGroupLayout;
    
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
    
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.multisample = multisampleState;
    pipelineDesc.depthStencil = &depthStencilState;
    
    m_pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    
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
    // Get device from render pass info
    WGPUDevice device = info.m_device;
    
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
        
        // Collect all triangles and their transforms
        std::vector<std::pair<entity_t, glm::mat4>> triangleTransforms;
        m_storage->ForEach([&triangleTransforms, transforms, projMat, viewMat](entity_t e, DummyTriangleComponent const&) {
            auto modelMat = transforms->GetOr(e, Transform::Identity()).AsMatrix();
            
            // Calculate MVP matrix: Projection * View * Model
            auto mvpMatrix = projMat * viewMat * modelMat;
            
            triangleTransforms.emplace_back(e, mvpMatrix);
        });
        
        // Upload all transforms to the buffer in one go
        for (size_t i = 0; i < triangleTransforms.size() && i < MAX_TRIANGLES; ++i) {
            uint32_t offset = static_cast<uint32_t>(i * TRANSFORM_ALIGNMENT);
            wgpuQueueWriteBuffer(info.m_queue, m_transformBuffer, offset, &triangleTransforms[i].second, sizeof(glm::mat4));
        }
        
        // Set pipeline once
        wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);
        
        // Render each triangle with its dynamic offset
        for (size_t i = 0; i < triangleTransforms.size() && i < MAX_TRIANGLES; ++i) {
            uint32_t dynamicOffset = static_cast<uint32_t>(i * TRANSFORM_ALIGNMENT);
            
            // Bind the group with dynamic offset for this triangle
            wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_transformBindGroup, 1, &dynamicOffset);
            
            // Draw the triangle
            wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
        }
    }
    
    return {};
}

WebgpuTriangleModule::WebgpuTriangleModule() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}