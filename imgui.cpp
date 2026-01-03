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

class ImguiModule final : 
    public EngineModule, 
    public IImguiProvider {
protected:
    ImDrawData m_drawDataToRender;
    ImGuiContext* m_context = nullptr;  

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
        graph.AddMessageNode([id = GetId()](JobContext& jobContext,
            In<Time> time,
            In<IOState> io,
            Pipe<KeyMessage, kImGuiInputPriority> keyMessage,
            Pipe<MouseButtonMessage, kImGuiInputPriority> mouseMessage, 
            Pipe<ImGuiContextObject, kPipePriorityFirst>) -> Error {

            if (io) {
                // Update ImGui IO state
                ImGuiIO& imgui_io = ImGui::GetIO();

                if (imgui_io.WantCaptureKeyboard) {
                    keyMessage.Handle([id](KeyMessage& msg) {
                        msg.m_captureId = id;   
                    });
                }
                if (imgui_io.WantCaptureMouse) {
                    mouseMessage.Handle([id](MouseButtonMessage& msg) {
                        msg.m_captureId = id;   
                    });
                }

                imgui_io.DisplaySize = ImVec2(
                    (float)io->m_display.m_framebufferSize.x, 
                    (float)io->m_display.m_framebufferSize.y
                );
                imgui_io.DeltaTime = (float)time->m_deltaTime;

                imgui_io.MousePos = ImVec2((float)io->m_mouse.m_cursorX, (float)io->m_mouse.m_cursorY);
                imgui_io.MouseDown[0] = io->m_mouse.IsButtonPressed(MouseButton::Left);
                imgui_io.MouseDown[1] = io->m_mouse.IsButtonPressed(MouseButton::Right);
                imgui_io.MouseDown[2] = io->m_mouse.IsButtonPressed(MouseButton::Middle);
                imgui_io.MouseDown[3] = io->m_mouse.IsButtonPressed(MouseButton::Button4);
                imgui_io.MouseDown[4] = io->m_mouse.IsButtonPressed(MouseButton::Button5);

                for (int i = 0; i < kKeyCount; ++i) {
                    ImGuiKey imguiKey = OkamiKeyToImGuiKey(static_cast<Key>(i));
                    if (imguiKey != ImGuiKey_None) {
                        imgui_io.AddKeyEvent(imguiKey, io->m_keyboard.m_keyStates[i]);
                    }
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

    Error ReceiveMessagesImpl(MessageBus& bus) {
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
