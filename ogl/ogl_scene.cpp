#include "ogl_scene.hpp"

#include "camera.hpp"
#include "transform.hpp"
#include "../light.hpp"
#include "../renderer.hpp"

#include <glog/logging.h>

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

        m_depthPassProvider = context.m_interfaces.Query<IOGLDepthPassProvider>();
        OKAMI_ERROR_RETURN_IF(!m_depthPassProvider,
            "IOGLDepthPassProvider interface not available for OGLSceneModule");

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
            .u_ambientLightColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
            .u_lightCount = glm::uvec4(0, 0, 0, 0)
        };

        int lightIdx = 0;
        registry.view<AmbientLightComponent>().each(
            [&](auto /*entity*/, AmbientLightComponent const& light) {
                lighting.u_ambientLightColor += glm::vec4(glm::vec3(light.m_color) * light.m_intensity, 0.0f);
            });

        registry.view<PointLightComponent>().each(
            [&](auto /*entity*/, PointLightComponent const& light) {
                if (lightIdx >= MAX_LIGHTS) return;
                lighting.u_lights[lightIdx++] = glsl::Light{
                    .u_direction      = glm::vec4(0.0f),
                    .u_positionRadius = glm::vec4(light.m_position, light.m_range),
                    .u_color          = glm::vec4(glm::vec3(light.m_color) * light.m_intensity, 1.0f),
                    .u_type           = glm::uvec4(POINT_LIGHT, 0, 0, 0)
                };
            });
        registry.view<DirectionalLightComponent>().each(
            [&](auto /*entity*/, DirectionalLightComponent const& light) {
                if (lightIdx >= MAX_LIGHTS) return;
                lighting.u_lights[lightIdx++] = glsl::Light{
                    .u_direction      = glm::vec4(glm::normalize(light.m_direction), 0.0f),
                    .u_positionRadius = glm::vec4(0.0f),
                    .u_color          = glm::vec4(glm::vec3(light.m_color) * light.m_intensity, 1.0f),
                    .u_type           = glm::uvec4(DIRECTIONAL_LIGHT, 0, 0, 0)
                };
            });
        lighting.u_lightCount.x = static_cast<unsigned int>(lightIdx);

        // Default tonemap parameters; overridden by any PostProcessComponent in the registry.
        PostProcessComponent pp;
        registry.view<PostProcessComponent>().each(
            [&](auto /*entity*/, PostProcessComponent const& comp) { pp = comp; });

        auto tonemap = glsl::TonemapGlobals{
            .u_tonemapABCD        = glm::vec4(pp.m_tonemapA, pp.m_tonemapB, pp.m_tonemapC, pp.m_tonemapD),
            .u_tonemapEFExposureW = glm::vec4(pp.m_tonemapE, pp.m_tonemapF, pp.m_exposure, pp.m_whitePoint)
        };

        return glsl::SceneGlobals{
            .u_camera   = glslCamera,
            .u_lighting = lighting,
            .u_tonemap  = tonemap,
            .u_shadow   = [&]() {
            glsl::ShadowGlobals s{};
            static const ShadowConfig kDefaultShadow;
            auto const& shadowCfg = registry.ctx().contains<ShadowConfig>()
                ? registry.ctx().get<ShadowConfig>() : kDefaultShadow;
            s.u_shadowBiasBase  = static_cast<float>(shadowCfg.m_shadowBiasBase);
            s.u_shadowBiasSlope = static_cast<float>(shadowCfg.m_shadowBiasSlope);
            s.u_shadowBiasMax   = static_cast<float>(shadowCfg.m_shadowBiasMax);
            s.u_shadowPad       = 0.0f;
            auto const& cascades = m_depthPassProvider->GetCurrentCascades();
            for (int i = 0; i < NUM_SHADOW_CASCADES; ++i)
                s.u_cascadeViewProj[i] = cascades.u_cascadeViewProj[i];
            s.u_cascadeSplits = m_depthPassProvider->GetCurrentCascadeSplits();
            return s;
        }(),
        .u_debug = [&]() {
            auto const* dbgCfg = registry.ctx().find<RenderDebugConfig>();
            return glm::uvec4(dbgCfg ? static_cast<uint32_t>(dbgCfg->m_mode) : 0u, 0u, 0u, 0u);
        }(),
        .u_frameIndex = glm::uvec4(m_frameIndex, 0u, 0u, 0u)
        };
    }

    Error OGLSceneModule::ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) {
        m_frameIndex++;
        return {};
    }

    void OGLSceneModule::SetSceneGlobals(glsl::SceneGlobals const& globals) {
        m_sceneUBO.Write(globals);
    }
} // namespace okami
