#pragma once

#include "../sample.hpp"
#include "gltf_scene.hpp"
#include "paths.hpp"
#include "sky.hpp"
#include "camera.hpp"
#include "camera_controllers.hpp"
#include "ogl/ogl_renderer.hpp"
#include "io.hpp"
#include "light.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glog/logging.h>

namespace sample_sponza {

// ─────────────────────────────────────────────────────────────────────────────
// Sample
// ─────────────────────────────────────────────────────────────────────────────

class SponzaSample : public okami::Sample {
public:
    // When true, texture URIs from the GLTF are rewritten to .ktx2.
    bool m_useKtx2 = true;

    void SetupModules(
        okami::Engine& en,
        std::optional<okami::HeadlessGLParams> headless = {}) override
    {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
        en.CreateModule<okami::CameraControllerModuleFactory>();
    }

    void SetupScene(okami::Engine& en) override {
        using namespace okami;

        auto lightDir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f));

        // ── Load GLTF scene prototype ──────────────────────────────────────
        auto gltfPath   = GetSampleAssetPath("sponza/Sponza.gltf");
        auto sceneResult = GltfScene::FromFile(gltfPath);
        if (!sceneResult) {
            LOG(ERROR) << "[Sponza] " << sceneResult.error();
            return;
        }

        SpawnGltfScene(en, std::move(sceneResult->m_data));

        // ── Sky ────────────────────────────────────────────────────────────
        {
            auto skyMaterial = en.CreateMaterial(SkyAtmosphereMaterial{
                // Sun sitting low on the horizon towards positive X
                .sunPosition = -lightDir,
                .turbidity   = 1.0f,
            });

            auto skyEntity = en.CreateEntity(kNullEntity, "Sky");
            en.AddComponent(skyEntity, SkyComponent{ .m_skyMaterial = skyMaterial });
        }

        // ── Lights ──────────────────────────────────────────────────────────
        {
            auto dirLightEntity = en.CreateEntity(kNullEntity, "Directional Light");
            en.AddComponent(dirLightEntity, DirectionalLightComponent{
                .m_direction = lightDir,
                .m_color     = color::White,
                .m_intensity = 2.0f,
                .b_castShadow = true
            });

            auto ambientLightEntity = en.CreateEntity(kNullEntity, "Ambient Light");
            en.AddComponent(ambientLightEntity, AmbientLightComponent{
                .m_color     = color::White,
                .m_intensity = 0.2f
            });
        }

        // ── First-person camera ────────────────────────────────────────────
        glm::quat initRot =
            glm::angleAxis(glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        auto cameraEntity = en.CreateEntity(kNullEntity, "Camera");
        en.AddComponent(cameraEntity,
            Camera::Perspective(glm::half_pi<float>(), 0.1f, 1000.0f));
        en.AddComponent(cameraEntity,
            Transform(glm::vec3(0.0f, 1.8f, 0.0f), initRot, 1.0f));
        en.AddComponent(cameraEntity, FirstPersonCameraControllerComponent{
            .m_moveSpeed       = 10.0f,
            .m_lookSensitivity = 0.002f,
        });
        en.SetActiveCamera(cameraEntity);
    }
};

} // namespace sample_sponza

