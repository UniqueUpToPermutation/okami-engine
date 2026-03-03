#pragma once

#include "engine.hpp"
#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "camera.hpp"
#include "camera_controllers.hpp"

namespace sample_zoo {

    inline void SetupScene(okami::Engine& en) {
        using namespace okami;

        auto textureHandle  = en.LoadTexture(GetAssetPath("test.ktx2"));
        auto geometryHandle = en.LoadGeometry(GetAssetPath("box.glb"));

        auto tri1Entity = en.CreateEntity();
        en.AddComponent(tri1Entity, DummyTriangleComponent{});
        en.AddComponent(tri1Entity, Transform::Translate(0.0f, 0.0f, 0.0f));

        auto tri2Entity = en.CreateEntity();
        en.AddComponent(tri2Entity, DummyTriangleComponent{});
        en.AddComponent(tri2Entity, Transform::Translate(0.25f, 0.25f, 0.15f));

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity, Camera::Perspective(glm::half_pi<float>(), 0.1f, 50.0f));
        en.AddComponent(cameraEntity, Transform::LookAt(
            glm::vec3(-1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        ));
        en.AddComponent(cameraEntity, OrbitCameraControllerComponent{});
        en.SetActiveCamera(cameraEntity);

        auto textureEntity = en.CreateEntity();
        en.AddComponent(textureEntity, SpriteComponent{ textureHandle });
        en.AddComponent(textureEntity, Transform(glm::vec3(0.0f, 0.0f, -0.25f), 1.0 / 64.0f));

        auto spriteEntity = en.CreateEntity();
        en.AddComponent(spriteEntity, SpriteComponent{
            .m_texture = textureHandle,
            .m_color   = color::Red,
        });
        en.AddComponent(spriteEntity, Transform::Translate(0.5f, 0.5f, 0.5f) * Transform::Scale(1.0 / 128.0f));

        auto geometryEntity = en.CreateEntity();
        en.AddComponent(geometryEntity, StaticMeshComponent{ geometryHandle });
        en.AddComponent(geometryEntity, Transform::Translate(0.5f, 0.1f, 0.1f) * Transform::Scale(0.25f));
    }

} // namespace sample_zoo
