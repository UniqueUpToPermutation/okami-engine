#include "ogl_headless.hpp"
#include "../renderer.hpp"

// Include GLAD before GLFW so that GLAD's function pointers are resolved
// before any GLFW GL-interop calls are made.  glad/gl.h must be included
// before any OpenGL headers (including GLFW3 when it auto-includes gl.h).
#include "glad/gl.h"

#include <GLFW/glfw3.h>

#include "../lodepng.h"
#include <glog/logging.h>

#include <vector>
#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace okami;

// ---------------------------------------------------------------------------
// HeadlessGLModule
// ---------------------------------------------------------------------------

class HeadlessGLModule final
    : public EngineModule
    , public IGLProvider
    , public IGUIModule
{
public:
    explicit HeadlessGLModule(HeadlessGLParams params)
        : m_params(std::move(params)) {}

    std::string GetName() const override { return "Headless GL Module"; }

    // IGLProvider ------------------------------------------------------------

    void NotifyNeedGLContext() override {
        // Context is always created for the headless module – nothing to do.
    }

    GL_GLADloadfunc GetGLLoaderFunction() const override {
        return glfwGetProcAddress;
    }

    // Called by OGLRendererModule at the end of each frame.
    // Instead of presenting to the screen we read the rendered pixels back
    // and write them to a PNG file in the configured capture directory.
    void SwapBuffers() override {
        int w = m_params.m_size.x;
        int h = m_params.m_size.y;

        if (!m_params.m_captureDir.empty()) {
            // Read RGBA pixels from the back buffer (bottom-left origin).
            std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            // Flip rows so that the image is top-left origin (PNG convention).
            std::vector<uint8_t> flipped(pixels.size());
            const size_t rowBytes = static_cast<size_t>(w * 4);
            for (int y = 0; y < h; ++y) {
                std::memcpy(
                    flipped.data() + static_cast<size_t>(y) * rowBytes,
                    pixels.data() + static_cast<size_t>(h - 1 - y) * rowBytes,
                    rowBytes);
            }

            std::filesystem::create_directories(m_params.m_captureDir);

            char nameBuf[32];
            std::snprintf(nameBuf, sizeof(nameBuf), "frame_%04zu.png", m_frameIndex);
            auto outPath = m_params.m_captureDir / nameBuf;

            unsigned err = lodepng_encode32_file(
                outPath.string().c_str(),
                flipped.data(),
                static_cast<unsigned>(w),
                static_cast<unsigned>(h));

            if (err) {
                LOG(WARNING) << "HeadlessGLModule: lodepng error " << err
                             << " (" << lodepng_error_text(err) << ") writing "
                             << outPath;
            }
        }

        ++m_frameIndex;
    }

    void SetSwapInterval(int /*interval*/) override {
        // No swap interval for offscreen rendering.
    }

    glm::ivec2 GetFramebufferSize() const override {
        return m_params.m_size;
    }

    // IGUIModule -------------------------------------------------------------

    Error MessagePump(InterfaceCollection& /*interfaces*/) override {
        // Keep GLFW happy on platforms that require regular event processing.
        glfwPollEvents();
        return {};
    }

    // EngineModule -----------------------------------------------------------

protected:
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IGLProvider>(this);
        interfaces.Register<IGUIModule>(this);
        return {};
    }

    Error StartupImpl(InitContext const& /*context*/) override {
        if (!glfwInit()) {
            return OKAMI_ERROR("HeadlessGLModule: Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE,   GLFW_FALSE); // hidden / off-screen
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(
            m_params.m_size.x,
            m_params.m_size.y,
            "Headless", nullptr, nullptr);

        OKAMI_ERROR_RETURN_IF(!m_window,
            "HeadlessGLModule: Failed to create invisible GLFW window");

        glfwMakeContextCurrent(m_window);
        return {};
    }

    void ShutdownImpl(InitContext const& /*context*/) override {
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }

private:
    HeadlessGLParams m_params;
    GLFWwindow*      m_window     = nullptr;
    size_t           m_frameIndex = 0;
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<EngineModule> HeadlessGLFWModuleFactory::operator()(HeadlessGLParams params) {
    return std::make_unique<HeadlessGLModule>(std::move(params));
}
