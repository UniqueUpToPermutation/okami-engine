#pragma once

#include "ogl_utils.hpp"
#include "ogl_geometry.hpp"
#include "ogl_material.hpp"

#include "../content.hpp"
#include "../transform.hpp"
#include "../renderer.hpp"
#include "../animation.hpp"

#include "shaders/scene.glsl"
#include "shaders/skinned_mesh.glsl"

namespace okami {
    // Renders all SkinnedMeshComponent entities.
    //
    // Each entity gets its own draw call (no instancing) because every entity
    // has unique joint matrices that must be uploaded to the JointMatricesBlock
    // UBO before drawing.  The UBO holds at most 256 joints (16 KB), which is
    // the minimum GL_MAX_UNIFORM_BLOCK_SIZE guaranteed by OpenGL 4.1.
    class OGLSkinnedMeshRenderer final :
        public EngineModule,
        public IOGLRenderModule {
    private:
        static constexpr int kMaxJoints = 256;

        OGLPipelineState m_pipelineState;

        // Instance VBO for per-entity transform data (reused each frame).
        UploadVertexBuffer<glsl::SkinnedMeshInstance> m_instanceVBO;

        // Joint matrices UBO (persistent, updated per draw call).
        GLBuffer m_jointUBO;

        // Bind points used by the forward program.
        enum class ForwardBindPoints : GLint {
            SceneGlobals   = 0,
            JointMatrices  = 1,
        };

        // Bind point used by the depth program.
        enum class DepthBindPoints : GLint {
            JointMatrices  = 0,
            Cascades       = 1,
        };

        static constexpr GLint kShadowMapUnit = 2;

        // Fallback material used when a SkinnedMeshComponent has no material set.
        MaterialHandle m_defaultMaterial;

        // Forward-pass program: skinned_mesh.vs + lambert.fs
        // Owned by OGLMaterialManager for lambert, but we compile a dedicated
        // one with the skinned vertex shader.
        GLProgram m_skinnedForwardProgram;

        // Depth-only program: skinned_mesh_depth.vs + static_mesh_depth.gs/.fs
        GLProgram m_depthProgram;

        OGLGeometryManager*          m_geometryManager      = nullptr;
        IOGLSceneGlobalsProvider*    m_sceneGlobalsProvider = nullptr;
        IOGLDepthPassProvider*       m_depthPassProvider    = nullptr;

        // Upload joint matrices for the given entity into m_jointUBO.
        // Returns false if the skeleton/skin data is not ready.
        bool UploadJointMatrices(
            entt::registry  const& registry,
            SkinnedMeshComponent const& mesh) const;

    protected:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

    public:
        explicit OGLSkinnedMeshRenderer(OGLGeometryManager* geometryManager);

        Error Pass(entt::registry const& registry, OGLPass const& pass) override;

        std::string GetName() const override;
    };
}
