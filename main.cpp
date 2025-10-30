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

    auto e2 = en.CreateEntity();
    en.AddComponent(e2, DummyTriangleComponent{});
    en.AddComponent(e2, Transform::Translate(0.0f, 0.0f, 0.0f));

    auto e3 = en.CreateEntity();
    en.AddComponent(e3, Camera::Identity());
    en.AddComponent(e3, Transform::Translate(0.5f, 0.0f, 0.0f));
    en.SetActiveCamera(e3);

    auto e4 = en.CreateEntity();
    en.AddComponent(e4, DummyTriangleComponent{});
    en.AddComponent(e4, Transform::Translate(0.5f, 0.5f, 0.0f));

    auto e5 = en.CreateEntity();
    en.AddComponent(e5, SpriteComponent{ textureHandle });
    en.AddComponent(e5, Transform::Scale(1.0 / 128.0f));

    auto e6 = en.CreateEntity();
    en.AddComponent(e6, SpriteComponent{ 
        .m_texture = textureHandle,
    });
    en.AddComponent(e6, Transform::Translate(0.5f, 0.5f, 0.0f) * Transform::Scale(1.0 / 128.0f));

    en.AddScript([e2](Time const& t, ModuleInterface& mi) {
        static double angle = 0.0f;
        angle += t.m_deltaTime;

        mi.m_messages.Send(UpdateComponentSignal<Transform>{
            e2, Transform::RotateZ(static_cast<float>(angle))
        });
    });

    en.Run();
    en.Shutdown();
}