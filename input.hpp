#pragma once

#include <array>
#include <glm/vec2.hpp>

namespace okami {
    enum class Key {
        Unknown,
        Space,
        Apostrophe,
        Comma,
        Minus,
        Period,
        Slash,
        Zero,
        One,
        Two,
        Three,
        Four,
        Five,
        Six,
        Seven,
        Eight,
        Nine,
        Semicolon,
        Equal,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        LeftBracket,
        Backslash,
        RightBracket,
        GraveAccent,
        World1,
        World2,
        Escape,
        Enter,
        Tab,
        Backspace,
        Insert,
        Delete,
        Right,
        Left,
        Down,
        Up,
        PageUp,
        PageDown,
        Home,
        End,
        CapsLock,
        ScrollLock,
        NumLock,
        PrintScreen,
        Pause,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,
        KP0,
        KP1,
        KP2,
        KP3,
        KP4,
        KP5,
        KP6,
        KP7,
        KP8,
        KP9,
        KPDecimal,
        KPDivide,
        KPMultiply,
        KPSubtract,
        KPAdd,
        KPEnter,
        KPEqual,
        LeftShift,
        LeftControl,
        LeftAlt,
        LeftSuper,
        RightShift,
        RightControl,
        RightAlt,
        RightSuper,
        Menu
    };

    constexpr size_t kKeyCount = static_cast<size_t>(Key::Menu) + 1;

    enum class MouseButton {
        Unknown,
        Left,
        Right,
        Middle,
        Button4,
        Button5,
        Button6,
        Button7,
        Button8
    };

    enum class Action {
        Press,
        Release,
        Repeat
    };

    constexpr int kNoCaptureId = -1;
    
    struct KeyMessage {
        Key m_key;
        Action m_action;
        int m_captureId = kNoCaptureId;
    };

    struct MouseButtonMessage {
        MouseButton m_button;
        Action m_action;
        int m_captureId = kNoCaptureId;
    };

    struct MousePosMessage {
        double m_x;
        double m_y;
        int m_captureId = kNoCaptureId;
    };

    struct ScrollMessage {
        double m_xOffset;
        double m_yOffset;
        int m_captureId = kNoCaptureId;
    };

    enum class CursorType {
        Standard,
        Hidden,
        Arrow,
        IBeam,
        Crosshair,
        Hand,
        HResize,
        VResize,
        ResizeAll,
        ResizeNESW,
        ResizeNWSE,
        NotAllowed,
        Unknown,

        Count
    };

    struct SetCursorMessage {
        CursorType m_cursorType;
    };

    struct KeyboardState {
        std::array<bool, kKeyCount> m_keyStates{};

        inline bool IsKeyPressed(Key key) const {
            int idx = static_cast<int>(key);
            if (idx >= 0 && idx < kKeyCount) {
                return m_keyStates[idx];
            }
            return false;
        }

        inline bool IsKeyReleased(Key key) const {
            return !IsKeyPressed(key);
        }
    };

    struct MouseState {
        std::unordered_map<MouseButton, bool> m_buttonStates;
        double m_cursorX = 0.0;
        double m_cursorY = 0.0;

        inline bool IsButtonPressed(MouseButton button) const {
            auto it = m_buttonStates.find(button);
            return it != m_buttonStates.end() && it->second;
        }

        inline bool IsButtonReleased(MouseButton button) const {
            auto it = m_buttonStates.find(button);
            return it == m_buttonStates.end() || !it->second;
        }
    };

    struct DisplayState {
        glm::ivec2 m_framebufferSize = { 0, 0 };
        glm::ivec2 m_windowSize = { 0, 0 };
        glm::ivec2 m_windowPosition = { 0, 0 };
        glm::vec2 m_contentScale = { 1.0f, 1.0f };
        bool m_focused = true;
        bool m_iconified = false;
    };

    struct IOState {
        KeyboardState m_keyboard;
        MouseState m_mouse;
        DisplayState m_display;
    };
}