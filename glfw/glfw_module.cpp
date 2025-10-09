#include "glfw_module.hpp"
#include "../renderer.hpp"
#include "../config.hpp"

#include <glog/logging.h>

#include <GLFW/glfw3.h>
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

using namespace okami;

static void glfw_errorCallback(int error, const char *description)
{
    LOG(ERROR) << "GLFW error " << error << ": " << description;
}

class GLFWModule final : 
    public EngineModule,
    public INativeWindowProvider,
    public IGLProvider {
private:
    GLFWwindow* m_window = nullptr;
    WindowConfig m_config;
    bool m_createContext = false;

public:
    void* GetNativeWindowHandle() const override {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        init.platformData.ndt = glfwGetX11Display();
        return (void*)(uintptr_t)glfwGetX11Window(m_window);
#elif defined(__APPLE__)
        return glfwGetCocoaWindow(m_window);
#elif defined(_WIN32)
        return glfwGetWin32Window(m_window);
#endif
    }

    void* GetNativeDisplayType() const override {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        return glfwGetX11Display();
#else
        return nullptr;
#endif
    }

    glm::ivec2 GetFramebufferSize() const override {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        return { width, height };
    }

protected:
    Error RegisterImpl(ModuleInterface& mi) override {
        mi.m_interfaces.Register<INativeWindowProvider>(this);
        mi.m_interfaces.Register<IGLProvider>(this);
        RegisterConfig<WindowConfig>(mi.m_interfaces, LOG_WRAP(WARNING));
        return {};
    }

    Error StartupImpl(ModuleInterface& mi) override {
        m_config = ReadConfig<WindowConfig>(mi.m_interfaces, LOG_WRAP(WARNING));

        glfwSetErrorCallback(glfw_errorCallback);
        
        if (!glfwInit()) {
            return Error("Failed to initialize GLFW");
        }

        if (m_createContext) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        } else {
            // No context by default - used with WebGPU/Vulkan/DirectX
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(
            m_config.backbufferWidth, 
            m_config.backbufferHeight, 
            m_config.windowTitle.c_str(), nullptr, nullptr);
        if (!m_window) {
            return Error("Failed to create GLFW window");
        }

        if (m_createContext) {
            glfwMakeContextCurrent(m_window);
        }

        return {};
    }

    void ShutdownImpl(ModuleInterface&) override {
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        
        glfwTerminate();
    }

    Error ProcessFrameImpl(Time const&, ModuleInterface& mi) override {
        glfwPollEvents();

        if (glfwWindowShouldClose(m_window)) {
            mi.m_messages.Send(SignalExit{});
        }

        return {};
    }

    Error MergeImpl() override {
        return {};
    }

public:
    std::string_view GetName() const override {
        return "GLFW Module";
    }

    void NotifyNeedGLContext() override {
        m_createContext = true;
    }

    GL_GLADloadfunc GetGLLoaderFunction() const override {
        return glfwGetProcAddress;
    }

    void SwapBuffers() override {
        glfwSwapBuffers(m_window);
    }

    void SetSwapInterval(int interval) override {
        glfwSwapInterval(interval);
    }
};

std::unique_ptr<EngineModule> GLFWModuleFactory::operator()() {
    return std::make_unique<GLFWModule>();
}