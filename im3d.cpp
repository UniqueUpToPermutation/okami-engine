#include "im3d.hpp"
#include "renderer.hpp"
#include "camera.hpp"
#include "transform.hpp"

#include <cmath>

using namespace okami;

class Im3dModule final : public EngineModule, public IIm3dProvider {
protected:
    Im3dContext m_contextToRender;

    IRenderModule*        m_renderModule   = nullptr;
    INativeWindowProvider* m_windowProvider = nullptr;
    entt::registry*       m_registry       = nullptr;

    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IIm3dProvider>(this);
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        context.m_messages.EnsurePort<Im3dContext>();
        m_renderModule   = context.m_interfaces.Query<IRenderModule>();
        m_windowProvider = context.m_interfaces.Query<INativeWindowProvider>();
        m_registry       = &context.m_registry;
        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error SendMessagesImpl(MessageBus& bus) override {
        Im3dContext ctx{ .m_context = std::make_unique<Im3d::Context>() };

        if (m_renderModule && m_registry) {
            entity_t activeCam = m_renderModule->GetActiveCamera();
            auto* camPtr = (activeCam != kNullEntity) ? m_registry->try_get<Camera>(activeCam) : nullptr;
            auto* txPtr  = (activeCam != kNullEntity) ? m_registry->try_get<Transform>(activeCam) : nullptr;

            glm::ivec2 viewport{0, 0};
            if (m_windowProvider)
                viewport = m_windowProvider->GetFramebufferSize();

            auto& appData = ctx->getAppData();

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
            appData.m_deltaTime = 1.0f / 60.0f;
        }

        // NewFrame: must be called after AppData is filled, before any Im3d draw calls
        ctx->reset();
        bus.Send(std::move(ctx));
        return {};
    }

    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        // This context now becomes the one to render
        bus.HandlePipe<Im3dContext>([this](Im3dContext& context) {
            context->endFrame();
            m_contextToRender = std::move(context);
        });

        return {};
    }

public:
    std::string GetName() const override {
        return "Im3d Provider Module";
    }

    Im3dContext const& GetIm3dContext() const override {
        return m_contextToRender;
    }
};

std::unique_ptr<EngineModule> Im3dModuleFactory::operator()() {
    return std::make_unique<Im3dModule>();
}