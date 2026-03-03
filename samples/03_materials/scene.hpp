#pragma once

#include "engine.hpp"
#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "sky.hpp"
#include "camera.hpp"
#include "camera_controllers.hpp"
#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"

#include <optional>

namespace sample_materials {

    inline void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
        en.CreateModule<okami::CameraControllerModuleFactory>();
    }

    inline void SetupScene(okami::Engine& en) {
        using namespace okami;

        auto textureHandle  = en.LoadTexture(GetAssetPath("test.ktx2"));
        auto geometryHandle = en.LoadGeometry(GetAssetPath("box.glb"));

        BasicTexturedMaterial material;
        material.m_colorTexture = textureHandle;
        auto materialHandle = en.CreateMaterial(material);

        auto boxEntity = en.CreateEntity();
        en.AddComponent(boxEntity, StaticMeshComponent{
            .m_geometry = geometryHandle,
            .m_material = materialHandle,
        });
        en.AddComponent(boxEntity, Transform::Identity());

        auto skyEntity = en.CreateEntity();
        en.AddComponent(skyEntity, SkyComponent{});

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity, Camera::Perspective(glm::half_pi<float>(), 0.1f, 50.0f));
        en.AddComponent(cameraEntity, Transform::LookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        ));
        en.AddComponent(cameraEntity, OrbitCameraControllerComponent{});
        en.SetActiveCamera(cameraEntity);
    }

} // namespace sample_materials
