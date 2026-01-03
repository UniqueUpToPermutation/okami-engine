#include "engine.hpp"

#include "ogl/ogl_renderer.hpp"
#include "glfw/glfw_module.hpp"

#include "transform.hpp"
#include "paths.hpp"
#include "geometry.hpp"

#include "imgui.hpp"

using namespace okami;

int main() {
    Engine en;

    RendererParams params;

    // Register renderer based on compile-time options
    en.CreateModule<GLFWModuleFactory>();
    en.CreateModule<ImGuiModuleFactory>();
    
    en.CreateModule<OGLRendererFactory>({}, params);

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    auto cameraEntity = en.CreateEntity();
    en.AddComponent(cameraEntity, Camera::Orthographic(2.25f, 1.0f, -1.0f));
    en.SetActiveCamera(cameraEntity);

    bool windowOpen = true;

    en.AddScript([&](JobGraph& graph, BuildGraphParams const& params) {
        graph.AddMessageNode([&](JobContext& jc, Pipe<ImGuiContextObject> imgui) -> Error {
            ImGui::ShowDemoWindow(&windowOpen);
            return {};
        });
    });

    en.Run();
    en.Shutdown();
}
