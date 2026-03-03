#pragma once

#include "module.hpp"

#include <filesystem>
#include <glm/vec2.hpp>

namespace okami {

    // Parameters for the headless (offscreen) GL context provider.
    // When passed to GLFWModuleFactory, an invisible window is created and each
    // SwapBuffers() call writes a PNG frame to m_captureDir instead of presenting.
    struct HeadlessGLParams {
        glm::ivec2            m_size       = {800, 600};
        std::filesystem::path m_captureDir;
    };

    struct GLFWModuleFactory {
        // Normal windowed mode.
        std::unique_ptr<EngineModule> operator()();
        // Headless / offscreen capture mode.
        std::unique_ptr<EngineModule> operator()(HeadlessGLParams params);
    };
}