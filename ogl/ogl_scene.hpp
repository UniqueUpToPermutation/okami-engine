#pragma once

#include "../module.hpp"
#include "ogl_utils.hpp"
#include "shaders/scene.glsl"

#include "renderer.hpp"
#include "entity_manager.hpp"

namespace okami {
    class OGLSceneModule final : 
        public EngineModule,
        public IOGLSceneGlobalsProvider {
    private:
        UniformBuffer<glsl::SceneGlobals> m_sceneUBO;
        IGLProvider* m_glProvider = nullptr;

    protected:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

    public:
        UniformBuffer<glsl::SceneGlobals> const& GetSceneGlobalsBuffer() const override;

        glsl::SceneGlobals GetSceneGlobals(entt::registry const& registry, entity_t activeCamera);
        void SetSceneGlobals(glsl::SceneGlobals const& globals);

        inline void UpdateSceneGlobals(entt::registry const& registry, entity_t activeCamera) {
            SetSceneGlobals(GetSceneGlobals(registry, activeCamera));
        }

        std::string GetName() const override {
            return "OpenGL Scene Module";
        }
    };
}
