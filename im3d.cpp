#include "im3d.hpp"
#include "renderer.hpp"
#include "camera.hpp"
#include "transform.hpp"
#include "input.hpp"

#include <imgui.h>
#include <cmath>

using namespace okami;

class Im3dModule final : public EngineModule, public IIm3dProvider {
protected:
    Im3dData m_renderData;

    IRenderModule*         m_renderModule   = nullptr;
    INativeWindowProvider* m_windowProvider = nullptr;
    entt::registry*        m_registry       = nullptr;

    // Persistent Im3d context — reused every frame so gizmo state
    // (m_activeId, m_hotId, m_keyDownPrev, drag vectors) survives across frames.
    std::shared_ptr<Im3d::Context> m_liveContext;

    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IIm3dProvider>(this);
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        context.m_messages.EnsurePort<Im3dContext>();
        m_renderModule   = context.m_interfaces.Query<IRenderModule>();
        m_windowProvider = context.m_interfaces.Query<INativeWindowProvider>();
        m_registry       = &context.m_registry;
        m_liveContext    = std::make_shared<Im3d::Context>();
        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
        graph.AddMessageNode(
            [this, &registry = params.m_registry](
                JobContext&,
                In<Time>         time,
                In<IOState>      ioState,
                In<DisplayState> display,
                Out<Im3dContext>  outCtx) -> Error
            {
                // Operate on the persistent context — do NOT create a new one.
                // Creating a fresh context every frame resets m_keyDownPrev, m_activeId,
                // m_hotId, and all drag state, breaking hover highlight and dragging.
                auto& appData = m_liveContext->getAppData();

                if (m_renderModule) {
                    entity_t activeCam = m_renderModule->GetActiveCamera();
                    auto* camPtr = (activeCam != kNullEntity) ? registry.try_get<Camera>(activeCam) : nullptr;
                    auto* txPtr  = (activeCam != kNullEntity) ? registry.try_get<Transform>(activeCam) : nullptr;

                    glm::ivec2 viewport{0, 0};
                    if (display)
                        viewport = display->m_framebufferSize;
                    else if (m_windowProvider)
                        viewport = m_windowProvider->GetFramebufferSize();

                    if (txPtr) {
                        glm::vec3 pos = txPtr->m_position;
                        glm::vec3 dir = txPtr->TransformVector(glm::vec3(0.0f, 0.0f, -1.0f));
                        appData.m_viewOrigin    = Im3d::Vec3(pos.x, pos.y, pos.z);
                        appData.m_viewDirection = Im3d::Vec3(dir.x, dir.y, dir.z);
                    }
                    if (viewport.x > 0 && viewport.y > 0)
                        appData.m_viewportSize = Im3d::Vec2((float)viewport.x, (float)viewport.y);

                    if (camPtr) {
                        if (auto* persp = std::get_if<PerspectiveProjection>(&camPtr->m_projection)) {
                            appData.m_projScaleY = std::tan(persp->m_fovY * 0.5f);
                            appData.m_projOrtho  = false;
                        } else if (auto* ortho = std::get_if<OrthographicProjection>(&camPtr->m_projection)) {
                            float h = ortho->m_height.value_or(
                                ortho->m_width.value_or((float)viewport.y) *
                                (viewport.x > 0 ? (float)viewport.y / (float)viewport.x : 1.0f));
                            appData.m_projScaleY = h * 0.5f;
                            appData.m_projOrtho  = true;
                        }
                    }

                    // Cursor ray: unproject mouse position into world space
                    if (ioState && camPtr && txPtr && viewport.x > 0 && viewport.y > 0) {
                        float mx = (float)ioState->m_mouse.m_cursorX;
                        float my = (float)ioState->m_mouse.m_cursorY;
                        float vw = (float)viewport.x;
                        float vh = (float)viewport.y;

                        glm::mat4 projMatrix = camPtr->GetProjectionMatrix(viewport.x, viewport.y, false);
                        glm::mat4 viewMatrix = txPtr->Inverse().AsMatrix();

                        glm::vec4 ndcRay  = glm::vec4(2.0f * mx / vw - 1.0f, 1.0f - 2.0f * my / vh, -1.0f, 1.0f);
                        glm::vec4 viewRay = glm::inverse(projMatrix) * ndcRay;
                        viewRay = glm::vec4(viewRay.x, viewRay.y, -1.0f, 0.0f);
                        glm::vec3 worldRay = glm::normalize(glm::vec3(glm::inverse(viewMatrix) * viewRay));

                        glm::vec3 pos = txPtr->m_position;
                        appData.m_cursorRayOrigin    = Im3d::Vec3(pos.x, pos.y, pos.z);
                        appData.m_cursorRayDirection = Im3d::Vec3(worldRay.x, worldRay.y, worldRay.z);
                    }
                }

                // Key/mouse states — skip if ImGui is consuming the mouse
                if (ioState && !ImGui::GetIO().WantCaptureMouse) {
                    appData.m_keyDown[Im3d::Mouse_Left]              = ioState->m_mouse.IsButtonPressed(MouseButton::Left);
                    appData.m_keyDown[Im3d::Action_GizmoTranslation] = ioState->m_keyboard.IsKeyPressed(Key::T);
                    appData.m_keyDown[Im3d::Action_GizmoRotation]    = ioState->m_keyboard.IsKeyPressed(Key::R);
                    appData.m_keyDown[Im3d::Action_GizmoScale]       = ioState->m_keyboard.IsKeyPressed(Key::S);
                    appData.m_keyDown[Im3d::Action_GizmoLocal]       = ioState->m_keyboard.IsKeyPressed(Key::L);
                }

                appData.m_deltaTime = time->GetDeltaTimeF();

                // NewFrame captures key states into m_keyDownCurr — must be called after all AppData is filled
                Im3d::SetContext(*m_liveContext);
                Im3d::NewFrame();

                // Send shared ownership — the renderer (one frame behind) and this frame's
                // draw path both reference the same Im3d::Context object.
                outCtx.Send(Im3dContext{ .m_context = m_liveContext });
                return {};
            });
        return {};
    }

    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        bus.HandlePipe<Im3dContext>([this](Im3dContext& context) {
            context->endFrame();

            // Deep-copy draw lists into m_renderData so the renderer has its own
            // independent snapshot — no shared state with m_liveContext.
            m_renderData.Clear();
            uint32_t count = m_liveContext->getDrawListCount();
            if (count > 0) {
                // Reserve full capacity first: pointers into m_renderData.m_data must
                // not be invalidated by reallocation after they are stored.
                size_t totalVerts = 0;
                for (uint32_t i = 0; i < count; ++i)
                    totalVerts += m_liveContext->getDrawLists()[i].m_vertexCount;
                m_renderData.m_data.reserve(totalVerts);

                for (uint32_t i = 0; i < count; ++i) {
                    auto const& dl = m_liveContext->getDrawLists()[i];
                    size_t offset = m_renderData.m_data.size();
                    m_renderData.m_data.insert(
                        m_renderData.m_data.end(),
                        dl.m_vertexData, dl.m_vertexData + dl.m_vertexCount);
                    m_renderData.m_drawLists.push_back(Im3d::DrawList{
                        .m_layerId     = dl.m_layerId,
                        .m_primType    = dl.m_primType,
                        .m_vertexData  = m_renderData.m_data.data() + offset,
                        .m_vertexCount = dl.m_vertexCount
                    });
                }
            }
        });
        return {};
    }

public:
    std::string GetName() const override {
        return "Im3d Provider Module";
    }

    Im3dData const& GetIm3dData() const override {
        return m_renderData;
    }
};

std::unique_ptr<EngineModule> Im3dModuleFactory::operator()() {
    return std::make_unique<Im3dModule>();
}