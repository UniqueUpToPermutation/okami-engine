#include <iostream>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "engine.hpp"

#include "webgpu/webgpu_renderer.hpp"
#include "glfw/glfw_module.hpp"

#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"

using namespace okami;

int main() {
    Engine en;

    // Register renderer based on compile-time options
    en.CreateRenderModule<GLFWModuleFactory>();
    en.CreateRenderModule<WebgpuRendererFactory>();

    Error err = en.Startup();

    // auto textureHandle = en.LoadResource<Texture>("test.ktx2");

    auto e2 = en.CreateEntity();
    en.AddComponent(e2, DummyTriangleComponent{});
    en.AddComponent(e2, Transform::Translate(0.0f, 0.0f, 0.0f));

    auto e3 = en.CreateEntity();
    en.AddComponent(e3, Camera::Identity());
    en.AddComponent(e3, Transform::Translate(0.5f, 0.0f, 0.0f));
    en.SetActiveCamera(e3);

    en.AddScript([e2](Time const& t, ModuleInterface& mi) {
        static double angle = 0.0f;
        angle += t.m_deltaTime;

        mi.m_messages.Send(UpdateComponentSignal<Transform>{
            e2, Transform::RotateZ(static_cast<float>(angle))
        });
    });

    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    en.Run();
    en.Shutdown();
}