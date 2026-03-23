#include "sample.hpp"

#include "editor.hpp"
#include "imgui.hpp"
#include "input.hpp"
#include "entity_manager.hpp"

namespace okami {

void InstallEditorModules(Engine& en) {
    en.CreateModule<ImGuiModuleFactory>();
    en.CreateModule<EditorModuleFactory>({}, EditorPropertiesCtx{ .b_showEditor = false });

    auto const& registry = en.GetRegistry();

    // Script: initialise EditorPropertiesCtx on the first frame, then toggle
    // b_showEditor whenever the tilde / grave-accent key is pressed.
    en.AddScript(
        [&registry](
            JobContext& con,
            In<KeyMessage>                              keys,
            Out<AddCtxSignal<EditorPropertiesCtx>>      addCtx,
            Out<UpdateCtxSignal<EditorPropertiesCtx>>   updateCtx) mutable -> Error
        {
            auto editorCtx = registry.ctx().get<EditorPropertiesCtx>();

            keys.Handle([&](KeyMessage const& msg) {
                if (msg.m_captureId != kNoCaptureId) {
                    return; // Already captured
                }

                if (msg.m_key == Key::GraveAccent && msg.m_action == Action::Press) {
                    editorCtx.b_showEditor = !editorCtx.b_showEditor;
                    updateCtx.Send(UpdateCtxSignal<EditorPropertiesCtx>{ editorCtx });
                }
            });

            return {};
        },
        "EditorToggle");
}

} // namespace okami
