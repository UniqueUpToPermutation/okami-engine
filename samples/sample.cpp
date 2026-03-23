#include "sample.hpp"

#include "editor.hpp"
#include "imgui.hpp"
#include "input.hpp"
#include "entity_manager.hpp"

namespace okami {

void InstallEditorModules(Engine& en) {
    en.CreateModule<ImGuiModuleFactory>();
    en.CreateModule<EditorModuleFactory>();

    // Script: initialise EditorPropertiesCtx on the first frame, then toggle
    // b_showEditor whenever the tilde / grave-accent key is pressed.
    en.AddScript(
        [visible = false](
            JobContext&,
            In<KeyMessage>                              keys,
            Out<AddCtxSignal<EditorPropertiesCtx>>      addCtx,
            Out<UpdateCtxSignal<EditorPropertiesCtx>>   updateCtx) mutable -> Error
        {
            // Seed the ctx on every frame; DefaultCtxMergeModule ignores it
            // once the ctx already exists.
            addCtx.Send(AddCtxSignal<EditorPropertiesCtx>{ EditorPropertiesCtx{ visible } });

            keys.Handle([&](KeyMessage const& msg) {
                if (msg.m_key == Key::GraveAccent && msg.m_action == Action::Press) {
                    visible = !visible;
                    updateCtx.Send(UpdateCtxSignal<EditorPropertiesCtx>{ EditorPropertiesCtx{ visible } });
                }
            });

            return {};
        },
        "EditorToggle");
}

} // namespace okami
