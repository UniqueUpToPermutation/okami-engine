#include "engine.hpp"

#include "ogl/ogl_renderer.hpp"
#include "glfw/glfw_module.hpp"

#include "transform.hpp"
#include "paths.hpp"
#include "geometry.hpp"

#include "im3d.hpp"

using namespace okami;

int main() {
    Engine en;

    RendererParams params;

    // Register renderer based on compile-time options
    en.CreateRenderModule<GLFWModuleFactory>();
    en.CreateRenderModule<OGLRendererFactory>({}, params);
    en.CreateUpdateModule<Im3dModuleFactory>();

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    auto cameraEntity = en.CreateEntity();
    en.AddComponent(cameraEntity, Camera::Orthographic(2.25f, 1.0f, -1.0f));
    en.SetActiveCamera(cameraEntity);

    en.AddScript([](Time const& time, ExecutionContext const& context) {
        context.m_graph->AddMessageNode([](JobContext& jc, Pipe<Im3dContext> im3d) -> Error {
            auto& im3dc = *im3d;

            im3dc->begin(Im3d::PrimitiveMode_Triangles);
            im3dc->vertex(Im3d::Vec3(-0.25f, 0.25f, 0.0f), 1.0, Im3d::Color_Magenta);
            im3dc->vertex(Im3d::Vec3(0.0f, 0.75f, 0.0f), 1.0, Im3d::Color_Yellow);
            im3dc->vertex(Im3d::Vec3(0.25f, 0.25f, 0.0f), 1.0, Im3d::Color_Cyan);
            im3dc->end();

            im3dc->begin(Im3d::PrimitiveMode_LineLoop);
            im3dc->vertex(Im3d::Vec3(-0.75f, -0.75f, 0.0f), 1.0, Im3d::Color_Red);
            im3dc->vertex(Im3d::Vec3(-0.5f, -0.25f, 0.0f), 1.0, Im3d::Color_Blue);
            im3dc->vertex(Im3d::Vec3(-0.25f, -0.75f, 0.0f), 1.0, Im3d::Color_Green);
            im3dc->end();

            im3dc->begin(Im3d::PrimitiveMode_Points);
            im3dc->vertex(Im3d::Vec3(0.25f, -0.75f, 0.0f), 1.0, Im3d::Color_Black);
            im3dc->vertex(Im3d::Vec3(0.5f, -0.25f, 0.0f), 1.0, Im3d::Color_Black);
            im3dc->vertex(Im3d::Vec3(0.75f, -0.75f, 0.0f), 1.0, Im3d::Color_Black);
            im3dc->end();

            return {};
        });
    });

    en.Run();
    en.Shutdown();
}
