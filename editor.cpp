#include "editor.hpp"

#include "imgui.hpp"
#include "meta.hpp"
#include "transform.hpp"
#include "entity_tree_view.hpp"

#include <entt/meta/resolve.hpp>
#include <entt/meta/meta.hpp>
#include <imgui.h>
#include <glm/gtc/quaternion.hpp>
#include <glog/logging.h>

#include <string>
#include <string_view>
#include <cstdio>

using namespace okami;
using namespace entt::literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Strip the leading namespace qualifier from a C++ RTTI type name so that
// e.g. "okami::Transform" becomes "Transform".
std::string_view StripNamespace(std::string_view name) {
    auto pos = name.rfind(':');
    return (pos != std::string_view::npos) ? name.substr(pos + 1) : name;
}

} // namespace

// ---------------------------------------------------------------------------
// EditorModule
// ---------------------------------------------------------------------------

class EditorModule final : public EngineModule {
    EditorPropertiesCtx m_initialCtx;
    entt::entity m_selectedEntity = entt::null;
    bool m_showScene     = false;
    bool m_showInspector = false;
    bool m_showContext   = false;

    // Draw one node and its subtree recursively.
    void DrawEntityNode(EntityTreeView const& tree,
                        entt::registry const& registry,
                        entity_t entity)
    {
        // Build a display label: prefer NameComponent, fall back to the id.
        std::string label;
        if (auto* name = registry.try_get<NameComponent>(entity)) {
            label = name->m_name;
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Entity %u", entt::to_integral(entity));
            label = buf;
        }

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (entity == m_selectedEntity)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (!tree.HasChildren(entity))
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool open = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(entity))),
            flags, "%s", label.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_selectedEntity = entity;

        if (open) {
            if (tree.HasChildren(entity)) {
                for (entity_t child : tree.Children(entity))
                    DrawEntityNode(tree, registry, child);
            }
            ImGui::TreePop();
        }
    }

    // ── Main menu bar ─────────────────────────────────────────────────────────
    void DrawMainMenuBar() {
        if (!ImGui::BeginMainMenuBar()) return;

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene",     nullptr, &m_showScene);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Context",   nullptr, &m_showContext);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // ── Scene window ─────────────────────────────────────────────────────────
    void DrawEntityList(entt::registry const& registry) {
        if (!m_showScene) return;
        ImGui::SetNextWindowSize({280, 420}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Scene", &m_showScene)) { ImGui::End(); return; }

        EntityTreeView tree(registry);

        // Find the root(s): entities with an EntityTreeComponent but no parent.
        // Normally there is exactly one root managed by EntityManager.
        bool anyRoot = false;
        for (auto&& [entity, treeComp] : registry.view<EntityTreeComponent>().each()) {
            if (treeComp.m_parent == kNullEntity) {
                anyRoot = true;
                DrawEntityNode(tree, registry, entity);
            }
        }

        if (!anyRoot) {
            ImGui::TextDisabled("(empty scene)");
        }

        ImGui::End();
    }

    // ── Inspector window ──────────────────────────────────────────────────────
    void DrawInspector(entt::registry const& registry, Out<UpdateComponentMetaSignal> updateComponent) {
        if (!m_showInspector) return;
        ImGui::SetNextWindowSize({360, 420}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Inspector", &m_showInspector)) { 
            ImGui::End(); 
            return; 
        }

        if (m_selectedEntity == kNullEntity ||
            !registry.valid(m_selectedEntity)) {
            ImGui::TextDisabled("No entity selected.");
            ImGui::End();
            return;
        }

        ImGui::Text("Entity %u", entt::to_integral(m_selectedEntity));
        ImGui::Separator();

        // Walk all storage pools; display each component the entity owns.
        for (auto&& [typeId, pool] : registry.storage()) {
            auto meta = entt::resolve(typeId);

            if (!meta) {
                continue;
            }

            auto fm = static_cast<MetaData*>(meta.custom());

            if (!pool.contains(m_selectedEntity)) {
                continue;
            }
           
            if (fm) {
                OKAMI_ASSERT(fm->IsComponent(), "Data should be component type");
                if (!fm->m_componentMetaData->b_showInEditor) { 
                    continue;
                }
            }

            // Resolve the type name via the meta system.
            std::string_view typeName = "(unknown)";
 
            if (meta) {
                typeName = StripNamespace(meta.info().name());
            }
            if (fm && fm->m_componentMetaData->m_displayName.size() > 0) {
                typeName = fm->m_componentMetaData->m_displayName;
            }

            char header[128];
            std::snprintf(header, sizeof(header), "%.*s",
                static_cast<int>(std::min(typeName.size(), size_t{120})),
                typeName.data());

            if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
                continue;
            }

            ImGui::Indent(12.f);
            DrawComponentDetail(registry, meta, m_selectedEntity,
                fm ? fm->m_componentMetaData->b_writeable : false, updateComponent);
            ImGui::Unindent(12.f);
        }

        ImGui::End();
    }

    // ── Shared field walker ───────────────────────────────────────────────────
    // Returns true if any field was modified.
    bool DrawFieldsOf(entt::meta_type meta, entt::meta_any& instance, bool parentWriteable) {
        bool anyField = false;
        bool anyModified = false;
        for (auto [fid, data] : meta.data()) {
            const char* label = "(field)";
            bool fieldWriteable = parentWriteable;
            FieldHint hint = FieldHint::Default;
            if (auto* fm = static_cast<FieldMeta*>(data.custom())) {
                if (!fm->b_showInEditor) continue;
                label = fm->m_displayName;
                if (fm->b_writeable.has_value())
                    fieldWriteable = *fm->b_writeable;
                hint = fm->m_hint;
            }

            entt::meta_any value = data.get(instance);
            if (!value) continue;
            anyField = true;

            auto vtype = value.type();

            // Use a unique ImGui ID per field to avoid conflicts
            ImGui::PushID(static_cast<int>(fid));

            if (vtype == entt::resolve<float>()) {
                float v = value.cast<float>();
                if (fieldWriteable) {
                    ImGui::Text("%-24s", label); ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::InputFloat("##", &v, 0.f, 0.f, "%.4f");
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        data.set(instance, v);
                        anyModified = true;
                    }
                } else {
                    ImGui::Text("%-24s  %.4f", label, v);
                }
            } else if (vtype == entt::resolve<double>()) {
                double v = value.cast<double>();
                if (fieldWriteable) {
                    float vf = static_cast<float>(v);
                    ImGui::Text("%-24s", label); ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::InputFloat("##", &vf, 0.f, 0.f, "%.6f");
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        data.set(instance, static_cast<double>(vf));
                        anyModified = true;
                    }
                } else {
                    ImGui::Text("%-24s  %.6f", label, static_cast<float>(v));
                }
            } else if (vtype == entt::resolve<int>()) {
                int v = value.cast<int>();
                if (fieldWriteable) {
                    ImGui::Text("%-24s", label); ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::InputInt("##", &v, 0, 0);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        data.set(instance, v);
                        anyModified = true;
                    }
                } else {
                    ImGui::Text("%-24s  %d", label, v);
                }
            } else if (vtype == entt::resolve<bool>()) {
                bool v = value.cast<bool>();
                if (fieldWriteable) {
                    if (ImGui::Checkbox(label, &v)) {
                        data.set(instance, v);
                        anyModified = true;
                    }
                } else {
                    ImGui::Text("%-24s  %s", label, v ? "true" : "false");
                }
            } else if (vtype == entt::resolve<glm::vec3>()) {
                glm::vec3 v = value.cast<glm::vec3>();
                if (hint == FieldHint::Color) {
                    if (fieldWriteable) {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1.f);
                        if (ImGui::ColorEdit3("##", &v.x, ImGuiColorEditFlags_Float)) {
                            data.set(instance, v);
                            anyModified = true;
                        }
                    } else {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::ColorButton("##", ImVec4(v.x, v.y, v.z, 1.f), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoTooltip);
                    }
                } else if (hint == FieldHint::Direction) {
                    // Normalise on display; normalize back to unit sphere after editing.
                    // Use the return value (fires every drag frame) rather than
                    // IsItemDeactivatedAfterEdit — sliders have no internal buffer, so
                    // n would reset to the old pool value on the release frame.
                    glm::vec3 n = glm::length(v) > 0.f ? glm::normalize(v) : v;
                    if (fieldWriteable) {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1.f);
                        if (ImGui::SliderFloat3("##", &n.x, -1.f, 1.f, "%.3f")) {
                            glm::vec3 result = glm::length(n) > 0.f ? glm::normalize(n) : n;
                            data.set(instance, result);
                            anyModified = true;
                        }
                    } else {
                        ImGui::Text("%-24s  %8.3f  %8.3f  %8.3f", label, n.x, n.y, n.z);
                    }
                } else {
                    if (fieldWriteable) {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1.f);
                        ImGui::InputFloat3("##", &v.x, "%.3f");
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            data.set(instance, v);
                            anyModified = true;
                        }
                    } else {
                        ImGui::Text("%-24s  %8.3f  %8.3f  %8.3f", label, v.x, v.y, v.z);
                    }
                }
            } else if (vtype == entt::resolve<glm::vec4>()) {
                glm::vec4 v = value.cast<glm::vec4>();
                if (hint == FieldHint::Color) {
                    if (fieldWriteable) {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1.f);
                        if (ImGui::ColorEdit4("##", &v.x, ImGuiColorEditFlags_Float)) {
                            data.set(instance, v);
                            anyModified = true;
                        }
                    } else {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::ColorButton("##", ImVec4(v.x, v.y, v.z, v.w), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoTooltip);
                    }
                } else {
                    if (fieldWriteable) {
                        ImGui::Text("%-24s", label); ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1.f);
                        ImGui::InputFloat4("##", &v.x, "%.3f");
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            data.set(instance, v);
                            anyModified = true;
                        }
                    } else {
                        ImGui::Text("%-24s  %6.3f  %6.3f  %6.3f  %6.3f", label, v.x, v.y, v.z, v.w);
                    }
                }
            } else {
                auto tname = vtype.info().name();
                ImGui::TextDisabled("%-24s  (%.*s)", label,
                    static_cast<int>(std::min(tname.size(), size_t{40})),
                    tname.data());
            }

            ImGui::PopID();
        }
        if (!anyField)
            ImGui::TextDisabled("(no registered fields)");
        return anyModified;
    }

    // ── Per-component detail ──────────────────────────────────────────────────
    void DrawComponentDetail(entt::registry const& registry,
                             entt::meta_type       meta,
                             entt::entity          entity,
                             bool                  parentWriteable,
                             Out<UpdateComponentMetaSignal> updateComponent)
    {
        if (!meta) return;

        // Transform: kept as a dedicated display for human-friendly euler angles.
        if (meta == entt::resolve<Transform>()) {
            auto const& t = registry.get<Transform>(entity);
            auto& p = t.m_position;
            auto  e = glm::eulerAngles(t.m_rotation);
            auto& s = t.m_scaleShear;
            float scale = (s[0][0] + s[1][1] + s[2][2]) / 3.0f;
            ImGui::Text("Position:  %8.3f  %8.3f  %8.3f", p.x, p.y, p.z);
            ImGui::Text("Rotation:  %8.3f  %8.3f  %8.3f  (euler °)",
                        glm::degrees(e.x), glm::degrees(e.y), glm::degrees(e.z));
            ImGui::Text("Scale:     %8.3f", scale);
            return;
        }

        entt::meta_any instance;
        for (auto&& [sid, pool] : registry.storage()) {
            if (sid == meta.id() && pool.contains(entity)) {
                // const_cast so from_void produces a non-const ref; data.set() requires it.
                instance = meta.from_void(const_cast<void*>(pool.value(entity)));
                break;
            }
        }
        if (!instance) return;

        // Make an independent value copy to edit; send it if anything changed.
        entt::meta_any copy = instance;
        if (DrawFieldsOf(meta, copy, parentWriteable)) {
            updateComponent.Send(UpdateComponentMetaSignal{ entity, std::move(copy) });
        }
    }

    // ── Context variables window ──────────────────────────────────────────────
    void DrawContextWindow(entt::registry const& registry, Out<UpdateCtxMetaSignal> updateCtx) {
        if (!m_showContext) return;
        ImGui::SetNextWindowSize({360, 380}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Context", &m_showContext)) { ImGui::End(); return; }

        bool any = false;
        for (auto const& metaType : GetRegisteredCtxs()) {
            auto meta = metaType.m_type;
            entt::meta_any value = GetCtxValue(registry, meta);
            auto fm = static_cast<MetaData*>(meta.custom());

            if (fm) {
                OKAMI_ASSERT(fm->IsCtx(), "Data should be ctx type");
                if (!fm->m_ctxMetaData->b_showInEditor) { 
                    continue;
                }
            }

            if (!value) { 
                continue;
            }

            any = true;

            std::string_view typeName = StripNamespace(meta.info().name());
            if (fm && fm->m_ctxMetaData->m_displayName.size() > 0) {
                typeName = fm->m_ctxMetaData->m_displayName;
            }

            char header[128];
            std::snprintf(header, sizeof(header), "%.*s",
                static_cast<int>(std::min(typeName.size(), size_t{120})),
                typeName.data());

            if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
                continue;
            }

            ImGui::Indent(12.f);
            bool ctxWriteable = fm ? fm->m_ctxMetaData->b_writeable : false;
            if (DrawFieldsOf(meta, value, ctxWriteable)) {
                updateCtx.Send(UpdateCtxMetaSignal{ value });
            }
            ImGui::Unindent(12.f);
        }

        if (!any) {
            ImGui::TextDisabled("(no context variables)");
        }

        ImGui::End();
    }

public:
    Error StartupImpl(InitContext const& con) override {
        auto& ctx = con.m_registry.ctx().emplace<EditorPropertiesCtx>(m_initialCtx);
        return {};
    }

    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
        graph.AddMessageNode(
            [this, &registry = params.m_registry](
                JobContext&, Pipe<ImGuiContextObject>,
                Out<UpdateComponentMetaSignal> updateComponent,
                Out<UpdateCtxMetaSignal> updateCtx) -> Error
            {
                auto* ctx = registry.ctx().find<EditorPropertiesCtx>();
                if (!ctx || !ctx->b_showEditor) return {};

                DrawMainMenuBar();
                DrawEntityList(registry);
                DrawInspector(registry, updateComponent);
                DrawContextWindow(registry, updateCtx);
                return {};
            });
        return {};
    }

    std::string GetName() const override { return "Editor"; }

    EditorModule(EditorPropertiesCtx const& ctx = {}) : m_initialCtx(ctx) {}
};

// ---------------------------------------------------------------------------

std::unique_ptr<EngineModule> EditorModuleFactory::operator()(EditorPropertiesCtx const& ctx) const {
    return std::make_unique<EditorModule>(ctx);
}
