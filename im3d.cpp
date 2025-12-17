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

    Error SendMessagesImpl(MessageBus& bus) override {
        // Send a context for next frame
        // People will pipe and write to this context
        bus.Send(Im3dContext{
            .m_context = std::make_unique<Im3d::Context>()
        });
        return {};
    }

    Error ReceiveMessagesImpl(MessageBus& bus) override {
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