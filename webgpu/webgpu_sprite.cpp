#include "webgpu_sprite.hpp"
#include "webgpu_utils.hpp"
#include "webgpu_texture.hpp"

#include "../transform.hpp"

#include <glog/logging.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>

using namespace okami;

Error WebgpuSpriteModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error WebgpuSpriteModule::StartupImpl(ModuleInterface& mi) {
    auto* renderer = mi.m_interfaces.Query<IWgpuRenderer>();
    if (!renderer) {
        return Error("IWgpuRenderer interface not available for WebGPU sprite module");
    }
    auto device = renderer->GetDevice();

    // Create camera uniform bind group layout
    WGPUBindGroupLayoutEntry cameraBindGroupLayoutEntry = {};
    cameraBindGroupLayoutEntry.binding = 0;
    cameraBindGroupLayoutEntry.visibility = WGPUShaderStage_Vertex;
    cameraBindGroupLayoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
    cameraBindGroupLayoutEntry.buffer.hasDynamicOffset = false;
    cameraBindGroupLayoutEntry.buffer.minBindingSize = sizeof(glm::mat4) * 2; // View + Projection matrices
    
    WGPUBindGroupLayoutDescriptor cameraBindGroupLayoutDesc = {};
    cameraBindGroupLayoutDesc.entryCount = 1;
    cameraBindGroupLayoutDesc.entries = &cameraBindGroupLayoutEntry;
    
    m_cameraBindGroupLayout = WgpuAutoPtr(wgpuDeviceCreateBindGroupLayout(device, &cameraBindGroupLayoutDesc));

    // Create texture bind group layout
    std::array<WGPUBindGroupLayoutEntry, 2> textureEntries = {};
    
    // Texture binding
    textureEntries[0].binding = 0;
    textureEntries[0].visibility = WGPUShaderStage_Fragment;
    textureEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
    textureEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    
    // Sampler binding
    textureEntries[1].binding = 1;
    textureEntries[1].visibility = WGPUShaderStage_Fragment;
    textureEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;
    
    WGPUBindGroupLayoutDescriptor textureBindGroupLayoutDesc = {};
    textureBindGroupLayoutDesc.entryCount = textureEntries.size();
    textureBindGroupLayoutDesc.entries = textureEntries.data();
    
    m_textureBindGroupLayout = WgpuAutoPtr(wgpuDeviceCreateBindGroupLayout(device, &textureBindGroupLayoutDesc));

    // Create camera uniform buffer
    WGPUBufferDescriptor cameraBufferDesc = {};
    cameraBufferDesc.size = sizeof(glm::mat4) * 2; // View + Projection matrices
    cameraBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    
    m_cameraBuffer = WgpuAutoPtr(wgpuDeviceCreateBuffer(device, &cameraBufferDesc));
    
    // Create camera bind group
    WGPUBindGroupEntry cameraBindGroupEntry = {};
    cameraBindGroupEntry.binding = 0;
    cameraBindGroupEntry.buffer = m_cameraBuffer.get();
    cameraBindGroupEntry.size = sizeof(glm::mat4) * 2;
    
    WGPUBindGroupDescriptor cameraBindGroupDesc = {};
    cameraBindGroupDesc.layout = m_cameraBindGroupLayout.get();
    cameraBindGroupDesc.entryCount = 1;
    cameraBindGroupDesc.entries = &cameraBindGroupEntry;

    m_cameraBindGroup = WgpuAutoPtr(wgpuDeviceCreateBindGroup(device, &cameraBindGroupDesc));

    // Create instance buffer for sprite data
    WGPUBufferDescriptor instanceBufferDesc = {};
    instanceBufferDesc.size = sizeof(SpriteInstance) * MAX_SPRITES;
    instanceBufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    
    m_instanceBuffer = WgpuAutoPtr(wgpuDeviceCreateBuffer(device, &instanceBufferDesc));

    // Create sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.maxAnisotropy = 1; // Must be at least 1
    
    m_sampler = WgpuAutoPtr(wgpuDeviceCreateSampler(device, &samplerDesc));

    // Load shader from file
    std::string shaderSource = LoadShaderFile("sprite.wgsl");
    if (shaderSource.empty()) {
        return Error("Failed to load sprite.wgsl shader file");
    }
    
    // Create shader module from loaded WGSL code
    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = shaderSource.c_str();
    
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslDesc.chain;
    
    WShaderModule shaderModule = WgpuAutoPtr(wgpuDeviceCreateShaderModule(device, &shaderDesc));

    // Define vertex layout for sprite instances
    std::array<WGPUVertexAttribute, 7> vertexAttributes = {};
    
    // Position (location 0)
    vertexAttributes[0].format = WGPUVertexFormat_Float32x3;
    vertexAttributes[0].offset = offsetof(SpriteInstance, position);
    vertexAttributes[0].shaderLocation = 0;
    
    // Rotation (location 1)
    vertexAttributes[1].format = WGPUVertexFormat_Float32;
    vertexAttributes[1].offset = offsetof(SpriteInstance, rotation);
    vertexAttributes[1].shaderLocation = 1;
    
    // Size (location 2)
    vertexAttributes[2].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[2].offset = offsetof(SpriteInstance, size);
    vertexAttributes[2].shaderLocation = 2;
    
    // UV0 (location 3)
    vertexAttributes[3].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[3].offset = offsetof(SpriteInstance, uv0);
    vertexAttributes[3].shaderLocation = 3;
    
    // UV1 (location 4)
    vertexAttributes[4].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[4].offset = offsetof(SpriteInstance, uv1);
    vertexAttributes[4].shaderLocation = 4;
    
    // Origin (location 5)
    vertexAttributes[5].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[5].offset = offsetof(SpriteInstance, origin);
    vertexAttributes[5].shaderLocation = 5;
    
    // Color (location 6)
    vertexAttributes[6].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[6].offset = offsetof(SpriteInstance, color);
    vertexAttributes[6].shaderLocation = 6;

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(SpriteInstance);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;
    vertexBufferLayout.attributeCount = vertexAttributes.size();
    vertexBufferLayout.attributes = vertexAttributes.data();

    // Create render pipeline
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = renderer->GetPreferredSwapchainFormat();
    colorTarget.writeMask = WGPUColorWriteMask_All;
    
    // Enable alpha blending for sprite transparency
    WGPUBlendState blendState = {};
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.alpha.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_One;
    blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    
    colorTarget.blend = &blendState;
    
    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule.get();
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    
    WGPUMultisampleState multisampleState = {};
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;
    
    // Configure depth-stencil state for sprite sorting
    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = true;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    
    // Properly initialize stencil states (even if we don't use stencil)
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
    
    // Create pipeline layout with both bind group layouts
    std::array<WGPUBindGroupLayout, 2> bindGroupLayouts = {
        m_cameraBindGroupLayout.get(),
        m_textureBindGroupLayout.get()
    };
    
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
    pipelineLayoutDesc.bindGroupLayouts = bindGroupLayouts.data();

    WPipelineLayout pipelineLayout = WgpuAutoPtr(wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc));

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout.get();
    pipelineDesc.vertex.module = shaderModule.get();
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    pipelineDesc.multisample = multisampleState;
    pipelineDesc.depthStencil = &depthStencilState;

    m_pipeline = WgpuAutoPtr(wgpuDeviceCreateRenderPipeline(device, &pipelineDesc));

    if (!m_pipeline) {
        return Error("Failed to create WebGPU sprite render pipeline");
    }
    
    LOG(INFO) << "WebGPU sprite module initialized successfully";

    return {};
}

void WebgpuSpriteModule::ShutdownImpl(ModuleInterface&) {
    m_textureBindGroup.reset();
    m_cameraBindGroup.reset();
    m_instanceBuffer.reset();
    m_cameraBuffer.reset();
    m_sampler.reset();
    m_textureBindGroupLayout.reset();
    m_cameraBindGroupLayout.reset();
    m_pipeline.reset();
}

Error WebgpuSpriteModule::ProcessFrameImpl(Time const&, ModuleInterface&) {
    return {};
}

Error WebgpuSpriteModule::MergeImpl() {
    return {};
}
    
std::string_view WebgpuSpriteModule::GetName() const {
    return "WebGPU Sprite Module";
}

SpriteInstance WebgpuSpriteModule::CreateSpriteInstance(const SpriteComponent& sprite, const Transform& transform) const {
    SpriteInstance instance = {};
    
    // Extract position from transform
    instance.position = transform.m_position;
    
    // Extract rotation (assuming 2D rotation around Z-axis)
    glm::vec3 eulerAngles = glm::eulerAngles(transform.m_rotation);
    instance.rotation = eulerAngles.z;
    
    // Extract size from transform scale (assuming uniform scaling for sprites)
    glm::vec3 scale = glm::vec3(transform.m_scaleShear[0][0], transform.m_scaleShear[1][1], transform.m_scaleShear[2][2]);
    
    // Use texture size if available, otherwise use transform scale
    if (sprite.m_texture && sprite.m_texture.IsLoaded()) {
        const auto& textureDesc = *sprite.m_texture; // This calls operator*() which returns TextureDesc&
        auto textureSize = glm::vec2(textureDesc.width, textureDesc.height);
        instance.size = textureSize * glm::vec2(scale.x, scale.y);
    } else {
        // Default size if no texture
        instance.size = glm::vec2(scale.x, scale.y);
    }
    
    // Set UV coordinates based on source rectangle
    if (sprite.m_sourceRect.has_value()) {
        const auto& rect = sprite.m_sourceRect.value();
        if (sprite.m_texture && sprite.m_texture.IsLoaded()) {
            const auto& textureDesc = *sprite.m_texture; // This calls operator*() which returns TextureDesc&
            float texWidth = static_cast<float>(textureDesc.width);
            float texHeight = static_cast<float>(textureDesc.height);
            instance.uv0 = rect.m_position / glm::vec2(texWidth, texHeight);
            instance.uv1 = (rect.m_position + rect.m_size) / glm::vec2(texWidth, texHeight);
        } else {
            instance.uv0 = glm::vec2(0.0f, 0.0f);
            instance.uv1 = glm::vec2(1.0f, 1.0f);
        }
    } else {
        // Use full texture
        instance.uv0 = glm::vec2(0.0f, 0.0f);
        instance.uv1 = glm::vec2(1.0f, 1.0f);
    }
    
    // Set origin point
    if (sprite.m_origin.has_value()) {
        instance.origin = sprite.m_origin.value();
    } else {
        // Default to center
        instance.origin = glm::vec2(0.5f, 0.5f);
    }
    
    // Set color
    instance.color = sprite.m_color;
    
    return instance;
}

void WebgpuSpriteModule::SortSpritesBackToFront(std::vector<std::pair<entity_t, SpriteInstance>>& sprites) const {
    // Sort by Z position (back to front for correct alpha blending)
    std::sort(sprites.begin(), sprites.end(), 
        [](const auto& a, const auto& b) {
            return a.second.position.z > b.second.position.z; // Higher Z values rendered first (back to front)
        });
}

Error WebgpuSpriteModule::Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) {
    if (m_storage->IsEmpty() || !m_pipeline) {
        return {}; // Nothing to render
    }

    // Get components interfaces
    auto transforms = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!transforms) {
        return Error("No IComponentView<Transform> available in WebGPUSpriteModule");
    }

    // Get render pass encoder and other info
    WGPUDevice device = info.m_device;
    WGPURenderPassEncoder renderPass = info.m_info;
    WGPUQueue queue = info.m_queue;

    // Upload camera matrices
    struct CameraUniforms {
        glm::mat4 view;
        glm::mat4 projection;
    } cameraUniforms;
    
    cameraUniforms.view = info.m_viewMatrix;
    cameraUniforms.projection = info.m_projMatrix;
    
    wgpuQueueWriteBuffer(queue, m_cameraBuffer.get(), 0, &cameraUniforms, sizeof(CameraUniforms));

    // Collect all sprites with their instance data
    std::vector<std::pair<entity_t, SpriteInstance>> spriteInstances;
    
    m_storage->ForEach([&](entity_t entity, const SpriteComponent& sprite) {
        // Only render sprites with valid textures
        if (!sprite.m_texture || !sprite.m_texture.IsLoaded()) {
            return;
        }
        
        auto transform = transforms->GetOr(entity, Transform::Identity());
        auto instance = CreateSpriteInstance(sprite, transform);
        spriteInstances.emplace_back(entity, instance);
    });

    if (spriteInstances.empty()) {
        return {}; // No valid sprites to render
    }

    // Sort sprites back to front for correct alpha blending
    SortSpritesBackToFront(spriteInstances);

    // Limit to maximum sprite count
    if (spriteInstances.size() > MAX_SPRITES) {
        spriteInstances.resize(MAX_SPRITES);
        LOG(WARNING) << "Too many sprites! Limiting to " << MAX_SPRITES << " sprites.";
    }

    // Upload sprite instance data
    std::vector<SpriteInstance> instanceData;
    instanceData.reserve(spriteInstances.size());
    for (const auto& pair : spriteInstances) {
        instanceData.push_back(pair.second);
    }
    
    wgpuQueueWriteBuffer(queue, m_instanceBuffer.get(), 0, 
                        instanceData.data(), instanceData.size() * sizeof(SpriteInstance));

    // Set pipeline and camera uniforms
    wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline.get());
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_cameraBindGroup.get(), 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_instanceBuffer.get(), 0, WGPU_WHOLE_SIZE);

    // Group sprites by texture to minimize bind group changes
    // Store bind groups to keep them alive until render pass is finished
    std::vector<WGPUBindGroup> tempBindGroups;
    ResHandle<Texture> currentTexture;
    uint32_t batchStart = 0;
    
    for (uint32_t i = 0; i <= static_cast<uint32_t>(spriteInstances.size()); ++i) {
        ResHandle<Texture> spriteTexture;
        if (i < spriteInstances.size()) {
            // Find the sprite component for this instance
            entity_t entity = spriteInstances[i].first;
            if (auto* spriteComp = m_storage->TryGet(entity)) {
                spriteTexture = spriteComp->m_texture;
            }
        }
        
        // Check if we need to flush the current batch
        if (i == spriteInstances.size() || spriteTexture.Ptr() != currentTexture.Ptr()) {
            if (currentTexture && currentTexture.IsLoaded() && i > batchStart) {
                // Get the WebGPU texture implementation from the texture module
                auto* textureImpl = m_textureModule->GetImpl(currentTexture);
                if (textureImpl && textureImpl->m_view.get()) {
                    // Create texture bind group for this batch
                    std::array<WGPUBindGroupEntry, 2> textureEntries = {};
                    
                    // Texture view entry
                    textureEntries[0].binding = 0;
                    textureEntries[0].textureView = textureImpl->m_view.get();
                    
                    // Sampler entry
                    textureEntries[1].binding = 1;
                    textureEntries[1].sampler = m_sampler.get();
                    
                    WGPUBindGroupDescriptor textureBindGroupDesc = {};
                    textureBindGroupDesc.layout = m_textureBindGroupLayout.get();
                    textureBindGroupDesc.entryCount = textureEntries.size();
                    textureBindGroupDesc.entries = textureEntries.data();
                    
                    // Create bind group and store it to keep alive
                    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &textureBindGroupDesc);
                    tempBindGroups.push_back(bindGroup);
                    
                    // Bind texture and render batch
                    wgpuRenderPassEncoderSetBindGroup(renderPass, 1, bindGroup, 0, nullptr);
                    
                    uint32_t instanceCount = i - batchStart;
                    wgpuRenderPassEncoderDraw(renderPass, 6, instanceCount, 0, batchStart); // 6 vertices for 2 triangles (quad)
                } else {
                    LOG(WARNING) << "Sprite texture implementation not available or texture view is null";
                }
            }
            
            currentTexture = spriteTexture;
            batchStart = i;
        }
    }
    
    // Clean up all temporary bind groups after rendering is complete
    // Note: This happens after the render pass commands are recorded but before they're executed
    // We need to keep them alive until after command buffer submission
    for (WGPUBindGroup bindGroup : tempBindGroups) {
        wgpuBindGroupRelease(bindGroup);
    }
    
    return {};
}

WebgpuSpriteModule::WebgpuSpriteModule(WebGpuTextureModule* textureModule) 
    : m_textureModule(textureModule) {
    if (!m_textureModule) {
        throw std::runtime_error("WebGpuTextureModule cannot be null");
    }
    m_storage = CreateChild<StorageModule<SpriteComponent>>();
}

WGPUBindGroup WebgpuSpriteModule::GetOrCreateTextureBindGroup(WGPUTextureView textureView, WGPUDevice device) {
    // Check if we already have a bind group for this texture view
    auto it = m_textureBindGroupCache.find(textureView);
    if (it != m_textureBindGroupCache.end()) {
        return it->second;
    }
    
    // Create new bind group for this texture
    std::array<WGPUBindGroupEntry, 2> textureEntries = {};
    
    // Texture view entry
    textureEntries[0].binding = 0;
    textureEntries[0].textureView = textureView;
    
    // Sampler entry  
    textureEntries[1].binding = 1;
    textureEntries[1].sampler = m_sampler.get();
    
    WGPUBindGroupDescriptor textureBindGroupDesc = {};
    textureBindGroupDesc.layout = m_textureBindGroupLayout.get();
    textureBindGroupDesc.entryCount = textureEntries.size();
    textureBindGroupDesc.entries = textureEntries.data();
    
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &textureBindGroupDesc);
    
    // Cache the bind group
    m_textureBindGroupCache[textureView] = bindGroup;
    
    return bindGroup;
}