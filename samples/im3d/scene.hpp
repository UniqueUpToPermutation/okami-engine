#pragma once

#include "engine.hpp"
#include "camera.hpp"
#include "im3d.hpp"
#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"

#include <optional>

namespace sample_im3d {
    inline void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::Im3dModuleFactory>();
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
    }

    inline void SetupScene(okami::Engine& en) {
        using namespace okami;

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity, Camera::Orthographic(3.0f, -1.0f, 1.0f));
        en.SetActiveCamera(cameraEntity);

        en.AddScript([](JobContext& /*jc*/, Pipe<Im3dContext> im3d) -> Error {
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
            im3dc->vertex(Im3d::Vec3(0.25f, -0.75f, 0.0f), 1.0, Im3d::Color_Red);
            im3dc->vertex(Im3d::Vec3(0.5f, -0.25f, 0.0f), 1.0, Im3d::Color_Blue);
            im3dc->vertex(Im3d::Vec3(0.75f, -0.75f, 0.0f), 1.0, Im3d::Color_Green);
            im3dc->end();

            return {};
        });
    }

} // namespace sample_im3d
