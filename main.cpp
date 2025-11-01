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

    auto geo = Geometry::LoadGLTF(GetAssetPath("box.glb"));
    auto indices = geo->TryAccessIndexed<uint16_t>(0);

    for (auto index : indices.value()) {
        std::cout << index << " ";
    }
    std::cout << "\n";

    std::cout << "Positions:\n";
    auto pos = geo->TryAccess<glm::vec3>(AttributeType::Position, 0);
    for (auto vertex : pos.value()) {
        std::cout << "(" << vertex.x << ", " << vertex.y << ", " << vertex.z << ")\n";
    }

    std::cout << "Normals:\n";
    auto normals = geo->TryAccess<glm::vec3>(AttributeType::Normal, 0);
    for (auto vertex : normals.value()) {
        std::cout << "(" << vertex.x << ", " << vertex.y << ", " << vertex.z << ")\n";
    }

    auto textureHandle = en.LoadResource<Texture>(GetAssetPath("test.ktx2"));
    auto geometryHandle = en.LoadResource<Geometry>(GetAssetPath("box.glb"));

    auto e2 = en.CreateEntity();
    en.AddComponent(e2, DummyTriangleComponent{});
    en.AddComponent(e2, Transform::Translate(0.0f, 0.0f, 0.0f));

    auto e4 = en.CreateEntity();
    en.AddComponent(e4, DummyTriangleComponent{});
    en.AddComponent(e4, Transform::Translate(0.25f, 0.25f, 0.15f));

    auto e3 = en.CreateEntity();
    en.AddComponent(e3, Camera::Orthographic(3.0f, 3.0f, -3.0f));
    en.AddComponent(e3, Transform::LookAt(
        glm::vec3(-1.0f, -1.0f, -1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));
    en.SetActiveCamera(e3);

    auto e5 = en.CreateEntity();
    en.AddComponent(e5, SpriteComponent{ textureHandle });
    en.AddComponent(e5, Transform(glm::vec3(0.0f, 0.0f, -0.25f), 1.0 / 128.0f));

    auto e6 = en.CreateEntity();
    en.AddComponent(e6, SpriteComponent{ 
        .m_texture = textureHandle,
        .m_color = color::Red,
    });
    en.AddComponent(e6, Transform::Translate(0.5f, 0.5f, 0.5f) * Transform::Scale(1.0 / 128.0f));
    
    auto e7 = en.CreateEntity();
    en.AddComponent(e7, StaticMeshComponent{ geometryHandle });
    en.AddComponent(e7, Transform::Translate(0.5f, 0.0f, 0.0f) * Transform::Scale(0.25f));

    en.AddScript([e2, e3](Time const& t, ModuleInterface& mi) {
        static double angle = 0.0f;
        angle += t.m_deltaTime;

        mi.m_messages.Send(UpdateComponentSignal<Transform>{
            e3, Transform::LookAt(
                glm::vec3(sin(angle), -1.0f, cos(angle)),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
            )
        });
    });

    en.Run();
    en.Shutdown();
}