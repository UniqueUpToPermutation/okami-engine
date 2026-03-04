#pragma once

#include "../sample.hpp"
#include "transform.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "renderer.hpp"
#include "camera.hpp"
#include "ogl/ogl_renderer.hpp"

namespace sample_hello_world {

class HelloWorldSample : public okami::Sample {
public:
    void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) override {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
    }

    void SetupScene(okami::Engine& en) override {
        using namespace okami;

        auto geometryHandle = en.LoadGeometry(GetSampleAssetPath("box.glb"));

        m_cameraEntity = en.CreateEntity();
        en.AddComponent(m_cameraEntity, Camera::Orthographic(3.0f, -3.0f, 3.0f));
        en.AddComponent(m_cameraEntity, Transform::LookAt(
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        ));
        en.SetActiveCamera(m_cameraEntity);

        auto meshEntity = en.CreateEntity();
        en.AddComponent(meshEntity, StaticMeshComponent{ geometryHandle });
        en.AddComponent(meshEntity, Transform::Scale(0.25f));

        entity_t cameraEntity = m_cameraEntity;
        en.AddScript([&en, cameraEntity](
            JobContext& /*job*/,
            In<Time> inTime,
            Out<UpdateComponentSignal<Transform>> outTransform) -> Error {

            auto transform = en.GetRegistry().get<Transform>(cameraEntity);
            outTransform.Send(UpdateComponentSignal<Transform>{
                .m_entity    = cameraEntity,
                .m_component = Transform::RotateY(0.5f * inTime->GetDeltaTimeF()) * transform
            });
            return {};
        });
    }

private:
    okami::entity_t m_cameraEntity = okami::kNullEntity;
};

} // namespace sample_hello_world
