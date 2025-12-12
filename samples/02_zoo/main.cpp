#include <iostream>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "engine.hpp"

#include "ogl/ogl_renderer.hpp"
#include "glfw/glfw_module.hpp"

#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "physics.hpp"

using namespace okami;

int main() {
    Engine en;

    RendererParams params;

    // Register renderer based on compile-time options
    en.CreateRenderModule<GLFWModuleFactory>();
    en.CreateRenderModule<OGLRendererFactory>({}, params);

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    auto textureHandle = en.LoadResource<Texture>(GetAssetPath("test.ktx2"));
    auto geometryHandle = en.LoadResource<Geometry>(GetAssetPath("box.glb"));

    auto tri1Entity = en.CreateEntity();
    en.AddComponent(tri1Entity, DummyTriangleComponent{});
    en.AddComponent(tri1Entity, Transform::Translate(0.0f, 0.0f, 0.0f));

    auto tri2Entity = en.CreateEntity();
    en.AddComponent(tri2Entity, DummyTriangleComponent{});
    en.AddComponent(tri2Entity, Transform::Translate(0.25f, 0.25f, 0.15f));

    auto cameraEntity = en.CreateEntity();
    en.AddComponent(cameraEntity, Camera::Orthographic(3.0f, 3.0f, -3.0f));
    en.AddComponent(cameraEntity, Transform::LookAt(
        glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));
    en.SetActiveCamera(cameraEntity);

    auto textureEntity = en.CreateEntity();
    en.AddComponent(textureEntity, SpriteComponent{ textureHandle });
    en.AddComponent(textureEntity, Transform(glm::vec3(0.0f, 0.0f, -0.25f), 1.0 / 64.0f));

    auto spriteEntity = en.CreateEntity();
    en.AddComponent(spriteEntity, SpriteComponent{ 
        .m_texture = textureHandle,
        .m_color = color::Red,
    });
    en.AddComponent(spriteEntity, Transform::Translate(0.5f, 0.5f, 0.5f) * Transform::Scale(1.0 / 128.0f));
    
    auto geometryEntity = en.CreateEntity();
    en.AddComponent(geometryEntity, StaticMeshComponent{ geometryHandle });
    en.AddComponent(geometryEntity, Transform::Translate(0.5f, 0.1f, 0.1f) * Transform::Scale(0.25f));

    en.AddScript([tri1Entity, cameraEntity](Time const& t, ExecutionContext const& context) {
        context.m_graph->AddMessageNode([t, cameraEntity](JobContext& ctx, PortOut<AddVelocityMessage> outVelocity) -> Error {
            outVelocity.Send(AddVelocityMessage{ .m_entity = cameraEntity, .m_velocity = glm::vec3(0.0f, 0.0f, 0.0f) });
            return Error{};
        });
    });

    en.Run();
    en.Shutdown();
}
