#pragma once

#include "engine.hpp"
#include "transform.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "renderer.hpp"
#include "camera.hpp"
#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"

#include <optional>

namespace sample_hello_world {

    inline void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
    }

    struct SceneContext {
        okami::entity_t cameraEntity = okami::kNullEntity;
    };

    inline SceneContext SetupScene(okami::Engine& en) {
        using namespace okami;

        auto geometryHandle = en.LoadGeometry(GetAssetPath("box.glb"));

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity, Camera::Orthographic(3.0f, -3.0f, 3.0f));
        en.AddComponent(cameraEntity, Transform::LookAt(
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        ));
        en.SetActiveCamera(cameraEntity);

        auto meshEntity = en.CreateEntity();
        en.AddComponent(meshEntity, StaticMeshComponent{ geometryHandle });
        en.AddComponent(meshEntity, Transform::Scale(0.25f));

        return { cameraEntity };
    }

} // namespace sample_hello_world
