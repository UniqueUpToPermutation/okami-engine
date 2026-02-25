#include "ogl_scene.hpp"

#include "camera.hpp"
#include "transform.hpp"

namespace okami {
    Error OGLSceneModule::RegisterImpl(InterfaceCollection& interfaces) {
        interfaces.Register<IOGLSceneGlobalsProvider>(this);

        m_glProvider = interfaces.Query<IGLProvider>();
        OKAMI_ERROR_RETURN_IF(!m_glProvider, "IGLProvider interface not available for OGLSceneModule");
        
        return {};
    }

    Error OGLSceneModule::StartupImpl(InitContext const& context) {
        auto ubo = UniformBuffer<glsl::SceneGlobals>::Create();
        OKAMI_ERROR_RETURN(ubo);
        m_sceneUBO = std::move(*ubo);
        return {};
    }

    UniformBuffer<glsl::SceneGlobals> const& OGLSceneModule::GetSceneGlobalsBuffer() const {
        return m_sceneUBO;
    }

    glsl::SceneGlobals OGLSceneModule::GetSceneGlobals(entt::registry const& registry, entity_t activeCamera) {
        auto cameraPtr = registry.try_get<Camera>(activeCamera);
        auto camera = cameraPtr ? *cameraPtr : Camera::Identity();

        auto framebufferSize = m_glProvider->GetFramebufferSize();
        glViewport(0, 0, framebufferSize.x, framebufferSize.y);

        auto cameraTransformPtr = registry.try_get<Transform>(activeCamera);
        auto cameraTransform = cameraTransformPtr ? *cameraTransformPtr : Transform::Identity();
        glm::mat4 viewMatrix = cameraTransform.Inverse().AsMatrix();
        glm::mat4 projMatrix = camera.GetProjectionMatrix(framebufferSize.x, framebufferSize.y, false);
        glm::mat4 viewProjMatrix = projMatrix * viewMatrix;

        auto glslCamera = glsl::CameraGlobals{
            .u_view = viewMatrix,
            .u_proj = projMatrix,
            .u_viewProj = viewProjMatrix,
            .u_invView = cameraTransform.AsMatrix(),
            .u_invProj = glm::inverse(projMatrix),
            .u_invViewProj = glm::inverse(viewProjMatrix),

            .u_viewport = glm::vec4(m_glProvider->GetFramebufferSize(), 0.0f, 0.0f),
            .u_cameraPosition = glm::vec4(cameraTransform.m_position, 1.0f),
            .u_cameraDirection = glm::vec4(cameraTransform.TransformVector(glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f)
        };

        auto lighting = glsl::LightingGlobals{
            .u_ambientLightColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f),
            .u_directionalLightDirection = glm::vec4(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)), 0.0f),
            .u_directionalLightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        };

        return glsl::SceneGlobals{
            .u_camera = glslCamera,
            .u_lighting = lighting
        };
    }

    void OGLSceneModule::SetSceneGlobals(glsl::SceneGlobals const& globals) {
        m_sceneUBO.Write(globals);
    }
}
