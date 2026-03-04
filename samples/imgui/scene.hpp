#pragma once

#include "../sample.hpp"
#include "camera.hpp"
#include "imgui.hpp"
#include "ogl/ogl_renderer.hpp"

namespace sample_imgui {

class ImGuiSample : public okami::Sample {
public:
    void SetupModules(okami::Engine& en, std::optional<okami::HeadlessGLParams> headless = {}) override {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::ImGuiModuleFactory>();
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
    }

    void SetupScene(okami::Engine& en) override {
        using namespace okami;

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity, Camera::Orthographic(2.25f, -1.0f, 1.0f));
        en.SetActiveCamera(cameraEntity);

        en.AddScript([](JobContext& /*jc*/, Pipe<ImGuiContextObject> imgui) -> Error {
            ImGui::GetIO().IniFilename = nullptr;
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::ShowDemoWindow(nullptr);
            return {};
        });
    }

    size_t GetTestFrameCount() const override {
        return 2; // Render two frames to ensure ImGui has time to initialize and render
    }
};

} // namespace sample_imgui
