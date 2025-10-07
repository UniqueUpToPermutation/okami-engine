#pragma once

#include "webgpu_utils.hpp"
#include "webgpu_texture.hpp"

#include "../module.hpp"
#include "../storage.hpp"
#include "../renderer.hpp"
#include "../transform.hpp"

#include <webgpu.h>
#include <vector>
#include <unordered_map>

namespace okami {
    // Sprite instance data structure that matches the WGSL shader
    struct SpriteInstance {
        glm::vec3 position;     // World position
        float rotation;         // Rotation angle in radians
        glm::vec2 size;         // Sprite size
        glm::vec2 uv0;          // Top-left UV coordinate
        glm::vec2 uv1;          // Bottom-right UV coordinate  
        glm::vec2 origin;       // Origin point (0-1 relative to sprite size)
        glm::vec4 color;        // Sprite tint color
    };

    class WebgpuSpriteModule : 
        public EngineModule,
        public IWgpuRenderModule {
    private:
        WRenderPipeline m_pipeline;
        StorageModule<SpriteComponent>* m_storage = nullptr;
        WebGpuTextureModule* m_textureModule = nullptr;
        
        // Vertex buffer for sprite instance data
        WBuffer m_instanceBuffer;
        
        // Texture and sampler resources
        WBindGroup m_textureBindGroup;
        WBindGroupLayout m_textureBindGroupLayout;
        WSampler m_sampler;
        
        // Uniform buffer resources for camera matrices
        WBuffer m_cameraBuffer;
        WBindGroup m_cameraBindGroup;
        WBindGroupLayout m_cameraBindGroupLayout;
        
        // Cache for texture bind groups to avoid creating them every frame
        std::unordered_map<WGPUTextureView, WBindGroup> m_textureBindGroupCache;
        
        // Constants for sprite management
        static constexpr uint32_t MAX_SPRITES = 1024;

    protected:
        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl() override;

    public:
        WebgpuSpriteModule(WebGpuTextureModule* textureModule);

        std::string_view GetName() const override;
        Error Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) override;

    private:
        // Helper method to convert SpriteComponent + Transform to SpriteInstance
        SpriteInstance CreateSpriteInstance(const SpriteComponent& sprite, const Transform& transform) const;
        
        // Helper method to sort sprites back-to-front based on Z position
        void SortSpritesBackToFront(std::vector<std::pair<entity_t, SpriteInstance>>& sprites) const;
        
        // Helper method to get or create a texture bind group
        WGPUBindGroup GetOrCreateTextureBindGroup(WGPUTextureView textureView, WGPUDevice device);
    };
}