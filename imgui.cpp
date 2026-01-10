#include "imgui.hpp"

#include "input.hpp"

using namespace okami;

ImGuiKey OkamiKeyToImGuiKey(Key key) {
    switch (key) {
        case Key::Tab: return ImGuiKey_Tab;
        case Key::Left: return ImGuiKey_LeftArrow;
        case Key::Right: return ImGuiKey_RightArrow;
        case Key::Up: return ImGuiKey_UpArrow;
        case Key::Down: return ImGuiKey_DownArrow;
        case Key::PageUp: return ImGuiKey_PageUp;
        case Key::PageDown: return ImGuiKey_PageDown;
        case Key::Home: return ImGuiKey_Home;
        case Key::End: return ImGuiKey_End;
        case Key::Insert: return ImGuiKey_Insert;
        case Key::Delete: return ImGuiKey_Delete;
        case Key::Backspace: return ImGuiKey_Backspace;
        case Key::Space: return ImGuiKey_Space;
        case Key::Enter: return ImGuiKey_Enter;
        case Key::Escape: return ImGuiKey_Escape;
        case Key::A: return ImGuiKey_A;
        case Key::B: return ImGuiKey_B;
        case Key::C: return ImGuiKey_C;
        case Key::D: return ImGuiKey_D;
        case Key::E: return ImGuiKey_E;
        case Key::F: return ImGuiKey_F;
        case Key::G: return ImGuiKey_G;
        case Key::H: return ImGuiKey_H;
        case Key::I: return ImGuiKey_I;
        case Key::J: return ImGuiKey_J;
        case Key::K: return ImGuiKey_K;
        case Key::L: return ImGuiKey_L;
        case Key::M: return ImGuiKey_M;
        case Key::N: return ImGuiKey_N;
        case Key::O: return ImGuiKey_O;
        case Key::P: return ImGuiKey_P;
        case Key::Q: return ImGuiKey_Q;
        case Key::R: return ImGuiKey_R;
        case Key::S: return ImGuiKey_S;
        case Key::T: return ImGuiKey_T;
        case Key::U: return ImGuiKey_U;
        case Key::V: return ImGuiKey_V;
        case Key::W: return ImGuiKey_W;
        case Key::X: return ImGuiKey_X;
        case Key::Y: return ImGuiKey_Y;
        case Key::Z: return ImGuiKey_Z;
        case Key::LeftControl: return ImGuiKey_LeftCtrl;
        case Key::RightControl: return ImGuiKey_RightCtrl;
        case Key::LeftShift: return ImGuiKey_LeftShift;
        case Key::RightShift: return ImGuiKey_RightShift;
        case Key::LeftAlt: return ImGuiKey_LeftAlt;
        case Key::RightAlt: return ImGuiKey_RightAlt;
        case Key::LeftSuper: return ImGuiKey_LeftSuper;
        case Key::RightSuper: return ImGuiKey_RightSuper;
        default: return ImGuiKey_None;
    }
}

CursorType ImGuiCursorToOkamiCursor(ImGuiMouseCursor cursor) {
    switch (cursor) {
        case ImGuiMouseCursor_Arrow: return CursorType::Arrow;
        case ImGuiMouseCursor_TextInput: return CursorType::IBeam;
        case ImGuiMouseCursor_ResizeAll: return CursorType::ResizeAll;
        case ImGuiMouseCursor_ResizeNS: return CursorType::VResize;
        case ImGuiMouseCursor_ResizeEW: return CursorType::HResize;
        case ImGuiMouseCursor_ResizeNESW: return CursorType::ResizeNESW;
        case ImGuiMouseCursor_ResizeNWSE: return CursorType::ResizeNWSE;
        case ImGuiMouseCursor_Hand: return CursorType::Hand;
        case ImGuiMouseCursor_NotAllowed: return CursorType::NotAllowed;
        default: return CursorType::Unknown;
    }
}

class ImguiModule final : 
    public EngineModule, 
    public IImguiProvider {
protected:
    ImDrawData m_drawDataToRender;
    ImGuiContext* m_context = nullptr;

    float m_lastScaleFactor = 1.0f;

    void ClearDrawData() {
        // Clear previous frame data to avoid memory leaks
        for (int i = 0; i < m_drawDataToRender.CmdLists.Size; i++) {
            delete m_drawDataToRender.CmdLists[i];
        }
        m_drawDataToRender.CmdLists.resize(0);
    }

    Error RegisterImpl(InterfaceCollection& a) {
        a.Register<IImguiProvider>(this);
        return {};
    }

    Error StartupImpl(InitContext const& ctx) {
        // Create ImGui context
        m_context = ImGui::CreateContext();
        ImGui::SetCurrentContext(m_context);

        // Setup ImGui style
        ImGui::StyleColorsDark();

        return {};
    }

    Error SendMessagesImpl(MessageBus& bus) {
        // Send a context for next frame
        bus.Send(ImGuiContextObject{
            .m_context = ImGui::GetCurrentContext()
        });
        return {};
    }

    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) {
        // Node to start a new ImGui frame
        graph.AddMessageNode([id = GetId(), &lastScaleFactor = m_lastScaleFactor](JobContext& jobContext,
            In<Time> time,
            In<IOState> io,
            Pipe<KeyMessage, kImGuiInputPriority> keyMessages,
            Pipe<MouseButtonMessage, kImGuiInputPriority> mouseMessages,
            Pipe<MousePosMessage> mousePosMessages,
            Pipe<ScrollMessage> scrollMessages,
            Pipe<CharMessage> charMessages,
            Out<SetCursorMessage> outSetCursor,
            Pipe<ImGuiContextObject, kPipePriorityFirst>) -> Error {

            if (io) {
                // Update ImGui IO state
                ImGuiIO& imgui_io = ImGui::GetIO();

                imgui_io.DisplaySize = ImVec2(
                    (float)io->m_display.m_framebufferSize.x, 
                    (float)io->m_display.m_framebufferSize.y
                );
                imgui_io.DeltaTime = (float)time->m_deltaTime;

                imgui_io.MousePos = ImVec2((float)io->m_mouse.m_cursorX, (float)io->m_mouse.m_cursorY);

                keyMessages.Handle([&imgui_io](KeyMessage& msg) {
                    if (msg.m_captureId != kNoCaptureId) {
                        return; // Already captured
                    }

                    ImGuiKey imguiKey = OkamiKeyToImGuiKey(msg.m_key);
                    if (imguiKey != ImGuiKey_None) {
                        bool isPressed = (msg.m_action == Action::Press || msg.m_action == Action::Repeat);
                        imgui_io.AddKeyEvent(imguiKey, isPressed);
                    }
                });

                mouseMessages.Handle([&imgui_io](MouseButtonMessage& msg) {
                    if (msg.m_captureId != kNoCaptureId) {
                        return; // Already captured
                    }

                    bool isPressed = (msg.m_action == Action::Press || msg.m_action == Action::Repeat);
                    switch (msg.m_button) {
                        case MouseButton::Left:
                            imgui_io.AddMouseButtonEvent(0, isPressed);
                            break;
                        case MouseButton::Right:
                            imgui_io.AddMouseButtonEvent(1, isPressed);
                            break;
                        case MouseButton::Middle:
                            imgui_io.AddMouseButtonEvent(2, isPressed);
                            break;
                        case MouseButton::Button4:
                            imgui_io.AddMouseButtonEvent(3, isPressed);
                            break;
                        case MouseButton::Button5:
                            imgui_io.AddMouseButtonEvent(4, isPressed);
                            break;
                        default:
                            break;
                    }
                });

                charMessages.Handle([&imgui_io](CharMessage& msg) {
                    if (msg.m_captureId != kNoCaptureId) {
                        return; // Already captured
                    }
                    imgui_io.AddInputCharacter((ImWchar)msg.m_char);
                });

                mousePosMessages.Handle([&imgui_io](MousePosMessage& msg) {
                    if (msg.m_captureId != kNoCaptureId) {
                        return; // Already captured
                    }
                    imgui_io.AddMousePosEvent((float)msg.m_x, (float)msg.m_y);
                });
                scrollMessages.Handle([&imgui_io](ScrollMessage& msg) {
                    if (msg.m_captureId != kNoCaptureId) {
                        return; // Already captured
                    }
                    imgui_io.AddMouseWheelEvent((float)msg.m_xOffset, (float)msg.m_yOffset);
                });

                // Scale everything for DPI changes
                if (lastScaleFactor != io->m_display.m_contentScale.x) {
                    auto newScaleFactor = io->m_display.m_contentScale.x;
                    ImGui::GetStyle().ScaleAllSizes(newScaleFactor / lastScaleFactor);
                    lastScaleFactor = newScaleFactor;
                }
                imgui_io.FontGlobalScale = lastScaleFactor;

                // Capture input messages if not captured by others
                if (imgui_io.WantCaptureKeyboard) {
                    keyMessages.Handle([id](KeyMessage& msg) {
                        if (msg.m_captureId != kNoCaptureId) {
                            return; // Already captured
                        }
                        msg.m_captureId = id;   
                    });
                }
                if (imgui_io.WantCaptureMouse) {
                    mouseMessages.Handle([id](MouseButtonMessage& msg) {
                        if (msg.m_captureId != kNoCaptureId) {
                            return; // Already captured
                        }
                        msg.m_captureId = id;   
                    });
                    mousePosMessages.Handle([id](MousePosMessage& msg) {
                        if (msg.m_captureId != kNoCaptureId) {
                            return; // Already captured
                        }
                        msg.m_captureId = id;   
                    });
                    scrollMessages.Handle([id](ScrollMessage& msg) {
                        if (msg.m_captureId != kNoCaptureId) {
                            return; // Already captured
                        }
                        msg.m_captureId = id;   
                    });
                    charMessages.Handle([id](CharMessage& msg) {
                        if (msg.m_captureId != kNoCaptureId) {
                            return; // Already captured
                        }
                        msg.m_captureId = id;   
                    });
                }

                // Update mouse cursor
                if (!(imgui_io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) {
                    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
                    SetCursorMessage setCursorMsg{ ImGuiCursorToOkamiCursor(imgui_cursor) };
                    outSetCursor.Send(setCursorMsg);
                }
            }

            ImGui::NewFrame();
            return {};
        });

        // Node to render ImGui
        graph.AddMessageNode([](JobContext& jobContext, Pipe<ImGuiContextObject, kPipePriorityLast>) -> Error {
            ImGui::Render();
            return {};
        });

        return {};
    }

    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const&) override {
        // This context now becomes the one to render
        bus.HandlePipe<ImGuiContextObject>([this](ImGuiContextObject& context) {
            auto drawData = ImGui::GetDrawData();
            
            ClearDrawData();

            if (drawData && drawData->Valid) {
                // Shallow copy the main structure properties
                m_drawDataToRender = *drawData;
                // Clear the pointers copied from source (which we don't own)
                m_drawDataToRender.CmdLists.resize(0);

                // Deep copy each command list
                for (int i = 0; i < drawData->CmdLists.Size; i++) {
                    ImDrawList* src_list = drawData->CmdLists[i];
                    // Create a new list without shared data (we only use it for storage)
                    ImDrawList* dst_list = new ImDrawList(nullptr);
                    
                    // Copy the buffers
                    dst_list->CmdBuffer = src_list->CmdBuffer;
                    dst_list->IdxBuffer = src_list->IdxBuffer;
                    dst_list->VtxBuffer = src_list->VtxBuffer;
                    dst_list->Flags = src_list->Flags;
                    
                    m_drawDataToRender.CmdLists.push_back(dst_list);
                }
            }
        });

        return {};
    }

public:
    ImDrawData& GetDrawData() override {
        return m_drawDataToRender;
    }

    std::string GetName() const override {
        return "ImGui Module";
    }

    ~ImguiModule() {
        ClearDrawData();
        ImGui::SetCurrentContext(nullptr);
        ImGui::DestroyContext(m_context);
    }
};

std::unique_ptr<EngineModule> ImGuiModuleFactory::operator()() {
    return std::make_unique<ImguiModule>();
}
