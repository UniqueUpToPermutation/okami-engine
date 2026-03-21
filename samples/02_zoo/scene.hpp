#pragma once

#include "../sample.hpp"
#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "camera.hpp"
#include "camera_controllers.hpp"
#include "ogl/ogl_renderer.hpp"

namespace sample_zoo {

class ZooSample : public okami::Sample {
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

        auto tri1Entity = en.CreateEntity(kNullEntity, "Triangle 1");
        en.AddComponent(tri1Entity, DummyTriangleComponent{});
        en.AddComponent(tri1Entity, Transform::Translate(0.0f, 0.0f, 0.0f));

        auto tri2Entity = en.CreateEntity(kNullEntity, "Triangle 2");
        en.AddComponent(tri2Entity, DummyTriangleComponent{});
        en.AddComponent(tri2Entity, Transform::Translate(0.25f, 0.25f, 0.15f));

        auto cameraEntity = en.CreateEntity(kNullEntity, "Camera");
        en.AddComponent(cameraEntity, Camera::Perspective(glm::half_pi<float>(), 0.1f, 50.0f));
        en.AddComponent(cameraEntity, Transform::LookAt(
            glm::vec3(-1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        ));
        en.AddComponent(cameraEntity, OrbitCameraControllerComponent{});
        en.SetActiveCamera(cameraEntity);

        auto textureEntity = en.CreateEntity(kNullEntity, "Sprite");
        en.AddComponent(textureEntity, SpriteComponent{ textureHandle });
        en.AddComponent(textureEntity, Transform(glm::vec3(0.0f, 0.0f, -0.25f), 1.0 / 64.0f));

        auto spriteEntity = en.CreateEntity(kNullEntity, "Red Sprite");
        en.AddComponent(spriteEntity, SpriteComponent{
            .m_texture = textureHandle,
            .m_color   = color::Red,
        });
        en.AddComponent(spriteEntity, Transform::Translate(0.5f, 0.5f, 0.5f) * Transform::Scale(1.0 / 128.0f));

        auto geometryEntity = en.CreateEntity(kNullEntity, "Static Mesh");
        en.AddComponent(geometryEntity, StaticMeshComponent{ geometryHandle });
        en.AddComponent(geometryEntity, Transform::Translate(0.5f, 0.1f, 0.1f) * Transform::Scale(0.25f));
    }
};

} // namespace sample_zoo
