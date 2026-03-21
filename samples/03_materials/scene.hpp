#pragma once

#include "../sample.hpp"
#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "sky.hpp"
#include "camera.hpp"
#include "camera_controllers.hpp"
#include "light.hpp"
#include "ogl/ogl_renderer.hpp"

namespace sample_materials {

class MaterialsSample : public okami::Sample {
public:
    void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) override {
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

        auto textureHandle  = en.LoadTexture(GetSampleAssetPath("test.ktx2"));
        auto geometryHandle = en.LoadGeometry(GetSampleAssetPath("box.glb"));

        LambertMaterial material;
        material.m_colorTexture = textureHandle;
        auto materialHandle = en.CreateMaterial(material);

        auto boxEntity = en.CreateEntity(kNullEntity, "Box");
        en.AddComponent(boxEntity, StaticMeshComponent{
            .m_geometry = geometryHandle,
            .m_material = materialHandle,
        });
        en.AddComponent(boxEntity, Transform::Identity());

        auto skyEntity = en.CreateEntity(kNullEntity, "Sky");
        en.AddComponent(skyEntity, SkyComponent{});

        auto cameraEntity = en.CreateEntity(kNullEntity, "Camera");
        en.AddComponent(cameraEntity, Camera::Perspective(glm::half_pi<float>(), 0.1f, 50.0f));
        en.AddComponent(cameraEntity, Transform::LookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        ));
        en.AddComponent(cameraEntity, OrbitCameraControllerComponent{});
        en.SetActiveCamera(cameraEntity);

        auto ambientLightEntity = en.CreateEntity(kNullEntity, "Ambient Light");
        en.AddComponent(ambientLightEntity, AmbientLightComponent{
            .m_color     = color::White,
            .m_intensity = 1.0f
        });
    }
};

} // namespace sample_materials
