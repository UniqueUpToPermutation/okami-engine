#pragma once

#include "engine.hpp"
#include "transform.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "renderer.hpp"
#include "camera.hpp"

namespace sample_hello_world {

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
