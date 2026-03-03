#pragma once

#include "engine.hpp"
#include "camera.hpp"
#include "imgui.hpp"
#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"

#include <optional>

namespace sample_imgui {
    inline void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::ImGuiModuleFactory>();
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
    }

    inline void SetupScene(okami::Engine& en) {
        using namespace okami;

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity, Camera::Orthographic(2.25f, -1.0f, 1.0f));
        en.SetActiveCamera(cameraEntity);

        en.AddScript([](JobContext& /*jc*/, Pipe<ImGuiContextObject> imgui) -> Error {
            ImGui::ShowDemoWindow(nullptr);
            return {};
        });
    }

} // namespace sample_imgui
