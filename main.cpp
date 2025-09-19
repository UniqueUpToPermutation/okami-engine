#include <iostream>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "engine.hpp"

#include "bgfx/bgfx_renderer.hpp"
#include "glfw/glfw_module.hpp"

#include "renderer.hpp"
#include "transform.hpp"

using namespace okami;

int main() {
    Engine en;

    // Register renderer based on compile-time options
    en.CreateRenderModule<GLFWModuleFactory>();
    en.CreateRenderModule<BgfxRendererFactory>();

    Error err = en.Startup();

    // Create scene
    auto e = en.CreateEntity();
    en.AddComponent(e, DummyTriangleComponent{});
    en.AddComponent(e, Transform::Translate(0.5f, 0.0f, 0.0f));

    e = en.CreateEntity();
    en.AddComponent(e, DummyTriangleComponent{});
    en.AddComponent(e, Transform::Translate(-0.5f, 0.0f, 0.0f));

    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    en.Run();
    en.Shutdown();
}