#include "engine.hpp"
#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"
#include "camera_controllers.hpp"
#include "scene.hpp"

using namespace okami;

int main() {
    Engine en;

    RendererParams params;

    en.CreateModule<GLFWModuleFactory>();
    en.CreateModule<OGLRendererFactory>({}, params);
    en.CreateModule<CameraControllerModuleFactory>();

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    sample_zoo::SetupScene(en);

    en.Run();
    en.Shutdown();
}
