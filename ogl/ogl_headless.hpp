#pragma once

#include "../module.hpp"

#include <filesystem>
#include <glm/vec2.hpp>

namespace okami {

    // Parameters for the headless (offscreen) OpenGL context provider.
    struct HeadlessGLParams {
        glm::ivec2 m_size = {800, 600};
        // Directory where captured PNG frames are written.
        // Each SwapBuffers() call writes frame_NNNN.png into this directory.
        // If empty, no images are written.
        std::filesystem::path m_captureDir;
    };

    // Creates an invisible GLFW window backed by an OpenGL 4.1 core-profile context.
    // SwapBuffers() reads the rendered pixels back via glReadPixels and saves them
    // as PNG files instead of presenting to a screen, enabling headless rendering
    // tests without a windowing system.
    //
    // Usage: en.CreateModule<HeadlessGLFWModuleFactory>({}, params);
    struct HeadlessGLFWModuleFactory {
        std::unique_ptr<EngineModule> operator()(HeadlessGLParams params = {});
    };

}
