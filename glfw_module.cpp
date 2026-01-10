#include "glfw_module.hpp"
#include "renderer.hpp"
#include "config.hpp"
#include "input.hpp"

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

Key GLFWToOkamiKey(int glfwKey) {
    switch (glfwKey) {
        case GLFW_KEY_UNKNOWN: return Key::Unknown;
        case GLFW_KEY_SPACE: return Key::Space;
        case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
        case GLFW_KEY_COMMA: return Key::Comma;
        case GLFW_KEY_MINUS: return Key::Minus;
        case GLFW_KEY_PERIOD: return Key::Period;
        case GLFW_KEY_SLASH: return Key::Slash;
        case GLFW_KEY_0: return Key::Zero;
        case GLFW_KEY_1: return Key::One;
        case GLFW_KEY_2: return Key::Two;
        case GLFW_KEY_3: return Key::Three;
        case GLFW_KEY_4: return Key::Four;
        case GLFW_KEY_5: return Key::Five;
        case GLFW_KEY_6: return Key::Six;
        case GLFW_KEY_7: return Key::Seven;
        case GLFW_KEY_8: return Key::Eight;
        case GLFW_KEY_9: return Key::Nine;
        case GLFW_KEY_SEMICOLON: return Key::Semicolon;
        case GLFW_KEY_EQUAL: return Key::Equal;
        case GLFW_KEY_A: return Key::A;
        case GLFW_KEY_B: return Key::B;
        case GLFW_KEY_C: return Key::C;
        case GLFW_KEY_D: return Key::D;
        case GLFW_KEY_E: return Key::E;
        case GLFW_KEY_F: return Key::F;
        case GLFW_KEY_G: return Key::G;
        case GLFW_KEY_H: return Key::H;
        case GLFW_KEY_I: return Key::I;
        case GLFW_KEY_J: return Key::J;
        case GLFW_KEY_K: return Key::K;
        case GLFW_KEY_L: return Key::L;
        case GLFW_KEY_M: return Key::M;
        case GLFW_KEY_N: return Key::N;
        case GLFW_KEY_O: return Key::O;
        case GLFW_KEY_P: return Key::P;
        case GLFW_KEY_Q: return Key::Q;
        case GLFW_KEY_R: return Key::R;
        case GLFW_KEY_S: return Key::S;
        case GLFW_KEY_T: return Key::T;
        case GLFW_KEY_U: return Key::U;
        case GLFW_KEY_V: return Key::V;
        case GLFW_KEY_W: return Key::W;
        case GLFW_KEY_X: return Key::X;
        case GLFW_KEY_Y: return Key::Y;
        case GLFW_KEY_Z: return Key::Z;
        case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
        case GLFW_KEY_BACKSLASH: return Key::Backslash;
        case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return Key::GraveAccent;
        case GLFW_KEY_WORLD_1: return Key::World1;
        case GLFW_KEY_WORLD_2: return Key::World2;
        case GLFW_KEY_ESCAPE: return Key::Escape;
        case GLFW_KEY_ENTER: return Key::Enter;
        case GLFW_KEY_TAB: return Key::Tab;
        case GLFW_KEY_BACKSPACE: return Key::Backspace;
        case GLFW_KEY_INSERT: return Key::Insert;
        case GLFW_KEY_DELETE: return Key::Delete;
        case GLFW_KEY_RIGHT: return Key::Right;
        case GLFW_KEY_LEFT: return Key::Left;
        case GLFW_KEY_DOWN: return Key::Down;
        case GLFW_KEY_UP: return Key::Up;
        case GLFW_KEY_PAGE_UP: return Key::PageUp;
        case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
        case GLFW_KEY_HOME: return Key::Home;
        case GLFW_KEY_END: return Key::End;
        case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
        case GLFW_KEY_SCROLL_LOCK: return Key::ScrollLock;
        case GLFW_KEY_NUM_LOCK: return Key::NumLock;
        case GLFW_KEY_PRINT_SCREEN: return Key::PrintScreen;
        case GLFW_KEY_PAUSE: return Key::Pause;
        case GLFW_KEY_F1: return Key::F1;
        case GLFW_KEY_F2: return Key::F2;
        case GLFW_KEY_F3: return Key::F3;
        case GLFW_KEY_F4: return Key::F4;
        case GLFW_KEY_F5: return Key::F5;
        case GLFW_KEY_F6: return Key::F6;
        case GLFW_KEY_F7: return Key::F7;
        case GLFW_KEY_F8: return Key::F8;
        case GLFW_KEY_F9: return Key::F9;
        case GLFW_KEY_F10: return Key::F10;
        case GLFW_KEY_F11: return Key::F11;
        case GLFW_KEY_F12: return Key::F12;
        case GLFW_KEY_F13: return Key::F13;
        case GLFW_KEY_F14: return Key::F14;
        case GLFW_KEY_F15: return Key::F15;
        case GLFW_KEY_F16: return Key::F16;
        case GLFW_KEY_F17: return Key::F17;
        case GLFW_KEY_F18: return Key::F18;
        case GLFW_KEY_F19: return Key::F19;
        case GLFW_KEY_F20: return Key::F20;
        case GLFW_KEY_F21: return Key::F21;
        case GLFW_KEY_F22: return Key::F22;
        case GLFW_KEY_F23: return Key::F23;
        case GLFW_KEY_F24: return Key::F24;
        case GLFW_KEY_F25: return Key::F25;
        case GLFW_KEY_KP_0: return Key::KP0;
        case GLFW_KEY_KP_1: return Key::KP1;
        case GLFW_KEY_KP_2: return Key::KP2;
        case GLFW_KEY_KP_3: return Key::KP3;
        case GLFW_KEY_KP_4: return Key::KP4;
        case GLFW_KEY_KP_5: return Key::KP5;
        case GLFW_KEY_KP_6: return Key::KP6;
        case GLFW_KEY_KP_7: return Key::KP7;
        case GLFW_KEY_KP_8: return Key::KP8;
        case GLFW_KEY_KP_9: return Key::KP9;
        case GLFW_KEY_KP_DECIMAL: return Key::KPDecimal;
        case GLFW_KEY_KP_DIVIDE: return Key::KPDivide;
        case GLFW_KEY_KP_MULTIPLY: return Key::KPMultiply;
        case GLFW_KEY_KP_SUBTRACT: return Key::KPSubtract;
        case GLFW_KEY_KP_ADD: return Key::KPAdd;
        case GLFW_KEY_KP_ENTER: return Key::KPEnter;
        case GLFW_KEY_KP_EQUAL: return Key::KPEqual;
        case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
        case GLFW_KEY_LEFT_CONTROL: return Key::LeftControl;
        case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
        case GLFW_KEY_LEFT_SUPER: return Key::LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
        case GLFW_KEY_RIGHT_CONTROL: return Key::RightControl;
        case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
        case GLFW_KEY_RIGHT_SUPER: return Key::RightSuper;
        case GLFW_KEY_MENU: return Key::Menu;
        default: return Key::Unknown;
    }
}

MouseButton GLFWToOkamiMouseButton(int glfwButton) {
    switch (glfwButton) {
        case GLFW_MOUSE_BUTTON_1: return MouseButton::Left;
        case GLFW_MOUSE_BUTTON_2: return MouseButton::Right;
        case GLFW_MOUSE_BUTTON_3: return MouseButton::Middle;
        case GLFW_MOUSE_BUTTON_4: return MouseButton::Button4;
        case GLFW_MOUSE_BUTTON_5: return MouseButton::Button5;
        case GLFW_MOUSE_BUTTON_6: return MouseButton::Button6;
        case GLFW_MOUSE_BUTTON_7: return MouseButton::Button7;
        case GLFW_MOUSE_BUTTON_8: return MouseButton::Button8;
        default: return MouseButton::Unknown;
    }
}

static void glfw_errorCallback(int error, const char *description)
{
    LOG(ERROR) << "GLFW error " << error << ": " << description;
}

class GLFWModule final : 
    public EngineModule,
    public INativeWindowProvider,
    public IGLProvider,
    public IGUIModule {
private:
    GLFWwindow* m_window = nullptr;
    WindowConfig m_config;
    bool m_createContext = false;

    KeyboardState m_keyboardState;
    MouseState m_mouseState;

    std::array<GLFWcursor*, static_cast<int>(CursorType::Count)> m_cursors = { nullptr };

    DefaultSignalHandler<KeyMessage> m_keySignalHandler;
    DefaultSignalHandler<MouseButtonMessage> m_mouseButtonSignalHandler;
    DefaultSignalHandler<MousePosMessage> m_mousePosSignalHandler;
    DefaultSignalHandler<ScrollMessage> m_scrollSignalHandler;
    DefaultSignalHandler<SetCursorMessage> m_setCursorSignalHandler;
    DefaultSignalHandler<CharMessage> m_charSignalHandler;

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
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<INativeWindowProvider>(this);
        interfaces.Register<IGLProvider>(this);
        interfaces.Register<IGUIModule>(this);

        interfaces.RegisterSignalHandler<KeyMessage>(&m_keySignalHandler);
        interfaces.RegisterSignalHandler<MouseButtonMessage>(&m_mouseButtonSignalHandler);
        interfaces.RegisterSignalHandler<MousePosMessage>(&m_mousePosSignalHandler);
        interfaces.RegisterSignalHandler<ScrollMessage>(&m_scrollSignalHandler);
        interfaces.RegisterSignalHandler<SetCursorMessage>(&m_setCursorSignalHandler);

        RegisterConfig<WindowConfig>(interfaces, LOG_WRAP(WARNING));
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        m_config = ReadConfig<WindowConfig>(context.m_interfaces, LOG_WRAP(WARNING));

        glfwSetErrorCallback(glfw_errorCallback);
        
        if (!glfwInit()) {
            return OKAMI_ERROR("Failed to initialize GLFW");
        }

        if (m_createContext) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
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
        OKAMI_ERROR_RETURN_IF(!m_window, "Failed to create GLFW window");

        glfwSetWindowUserPointer(m_window, this);

        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            GLFWModule* module = static_cast<GLFWModule*>(glfwGetWindowUserPointer(window));
            Key okamiKey = GLFWToOkamiKey(key);
            Action okamiAction = action == GLFW_PRESS ? Action::Press : action == GLFW_RELEASE ? Action::Release : Action::Repeat;
            if (okamiKey != Key::Unknown) {
                module->m_keySignalHandler.Send({okamiKey, okamiAction});
            }
        });

        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
            GLFWModule* module = static_cast<GLFWModule*>(glfwGetWindowUserPointer(window));
            MouseButton okamiButton = GLFWToOkamiMouseButton(button);
            Action okamiAction = action == GLFW_PRESS ? Action::Press : Action::Release;
            module->m_mouseButtonSignalHandler.Send({okamiButton, okamiAction});
        });

        glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
            GLFWModule* module = static_cast<GLFWModule*>(glfwGetWindowUserPointer(window));
            module->m_mousePosSignalHandler.Send({xpos, ypos});
        });

        glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
            GLFWModule* module = static_cast<GLFWModule*>(glfwGetWindowUserPointer(window));
            module->m_scrollSignalHandler.Send({xoffset, yoffset});
        });

        glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) {
            GLFWModule* module = static_cast<GLFWModule*>(glfwGetWindowUserPointer(window));
            module->m_charSignalHandler.Send({codepoint});
        });

        if (m_createContext) {
            glfwMakeContextCurrent(m_window);
        }

        // Create cursors
        m_cursors[static_cast<int>(CursorType::Arrow)] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        m_cursors[static_cast<int>(CursorType::IBeam)] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
        m_cursors[static_cast<int>(CursorType::Crosshair)] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
        m_cursors[static_cast<int>(CursorType::Hand)] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
        m_cursors[static_cast<int>(CursorType::HResize)] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
        m_cursors[static_cast<int>(CursorType::VResize)] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
        m_cursors[static_cast<int>(CursorType::ResizeAll)] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
        m_cursors[static_cast<int>(CursorType::ResizeNESW)] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
        m_cursors[static_cast<int>(CursorType::ResizeNWSE)] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
        m_cursors[static_cast<int>(CursorType::NotAllowed)] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);

        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
        for (auto& cursor : m_cursors) {
            if (cursor) {
                glfwDestroyCursor(cursor);
            }
            cursor = nullptr;
        }

        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        
        glfwTerminate();
    }
    
    // This will potentially run on a different thread
    Error MessagePump(InterfaceCollection& interfaces) override {
        glfwPollEvents();

        if (glfwWindowShouldClose(m_window)) {
            interfaces.SendSignal(SignalExit{});
        }

        m_setCursorSignalHandler.Handle([this](SetCursorMessage const& msg) {
            GLFWcursor* cursor = m_cursors[static_cast<int>(msg.m_cursorType)];
            glfwSetCursor(m_window, cursor);
            if (msg.m_cursorType == CursorType::Hidden) {
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            } else {
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        });

        return {};
    }

    Error SendMessagesImpl(MessageBus& bus) override {
        // Forward input events    
        m_keySignalHandler.HandleSpan([&](std::span<KeyMessage> messages) {
            bus.SendBatch<KeyMessage>(messages);
            for (const auto& msg : messages) {
                m_keyboardState.m_keyStates[static_cast<int>(msg.m_key)] = (msg.m_action == Action::Press || msg.m_action == Action::Repeat);
            }
        });
        m_mouseButtonSignalHandler.HandleSpan([&](std::span<MouseButtonMessage> messages) {
            bus.SendBatch<MouseButtonMessage>(messages);
            for (const auto& msg : messages) {
                m_mouseState.m_buttonStates[msg.m_button] = (msg.m_action == Action::Press);
            }
        });
        m_mousePosSignalHandler.HandleSpan([&](std::span<MousePosMessage> messages) {
            bus.SendBatch<MousePosMessage>(messages);
            for (const auto& msg : messages) {
                m_mouseState.m_cursorX = msg.m_x;
                m_mouseState.m_cursorY = msg.m_y;
            }
        });
        m_scrollSignalHandler.HandleSpan([&](std::span<ScrollMessage> messages) {
            bus.SendBatch<ScrollMessage>(messages);
        });
        m_charSignalHandler.HandleSpan([&](std::span<CharMessage> messages) {
            bus.SendBatch<CharMessage>(messages);
        });

        // Send IO state
        auto winPos = glm::ivec2{0, 0};
        glfwGetWindowPos(m_window, &winPos.x, &winPos.y);

        glm::vec2 contentScale{1.0f, 1.0f};
        glfwGetWindowContentScale(m_window, &contentScale.x, &contentScale.y);

        glm::ivec2 windowSize{0, 0};
        glfwGetWindowSize(m_window, &windowSize.x, &windowSize.y);

        glfwGetCursorPos(m_window, &m_mouseState.m_cursorX, &m_mouseState.m_cursorY);

        bus.Send<IOState>(IOState{ 
            .m_keyboard = m_keyboardState, 
            .m_mouse = m_mouseState,
            .m_display = DisplayState{
                .m_framebufferSize = GetFramebufferSize(),
                .m_windowSize = windowSize, 
                .m_windowPosition = winPos,
                .m_contentScale = contentScale,
                .m_focused = glfwGetWindowAttrib(m_window, GLFW_FOCUSED) != 0,
                .m_iconified = glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0
            }
        });

        return {};
    }

    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        // Forward cursor messages to GLFW
        bus.Handle<SetCursorMessage>([this](SetCursorMessage const& msg) {
            m_setCursorSignalHandler.Send(msg);
        });

        return {};
    }

public:
    std::string GetName() const override {
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