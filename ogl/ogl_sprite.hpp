#pragma once

#include "../renderer.hpp"
#include "../storage.hpp"
#include "../transform.hpp"
#include "../texture.hpp"

#include "ogl_utils.hpp"

#include "shaders/sprite.glsl"
#include "shaders/scene.glsl"

#include "ogl_texture.hpp"

namespace okami {
    class OGLSpriteRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    protected:
        OGLPipelineState m_pipelineState;

        GLProgram m_program;
        UploadVertexBuffer<glsl::SpriteInstance> m_instanceBuffer; // Instance buffer for sprite data
        
        // Uniform locations
        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;
        GLint u_texture = -1;

        // Component storage and views
        StorageModule<SpriteComponent>* m_storage = nullptr;
        IComponentView<Transform>* m_transformView = nullptr;
        OGLTextureManager* m_textureManager = nullptr;
        
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;
        void ShutdownImpl(InitContext const& context) override;

        Error ProcessFrameImpl(Time const& time, ExecutionContext const& context) override;
        Error MergeImpl(MergeContext const& context) override;
    
    public:
        OGLSpriteRenderer(OGLTextureManager* textureManager);

        Error Pass(OGLPass const& pass) override;

        std::string GetName() const override;
        
    private:
        // Helper method to convert SpriteComponent + Transform to SpriteInstance
        glsl::SpriteInstance CreateSpriteInstance(const SpriteComponent& sprite, const Transform& transform) const;
        
        // Helper method to sort sprites back-to-front based on Z position
        void SortSpritesBackToFront(std::vector<std::pair<entity_t, glsl::SpriteInstance>>& sprites) const;
    };
}