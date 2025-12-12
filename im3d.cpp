#include "im3d.hpp"

using namespace okami;

class Im3dModule final : public EngineModule, public IIm3dProvider {
protected:
    Im3dContext m_contextToRender;

    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IIm3dProvider>(this);
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        context.m_messages.EnsurePort<Im3dContext>();
        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error ProcessFrameImpl(Time const& time, ExecutionContext const& ec) override {
        // Send a context for next frame
        // People will pipe and write to this context
        ec.m_messages->Send(Im3dContext{
            .m_context = std::make_unique<Im3d::Context>()
        });
        return {};
    }

    Error MergeImpl(MergeContext const& context) override {
        auto port = context.m_messages.GetPort<Im3dContext>();

        // This context now becomes the one to render
        port->HandlePipeSingle([this](Im3dContext& context) {
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