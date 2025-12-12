#include "im3d.hpp"

using namespace okami;

class Im3dModule final : public EngineModule, public IIm3dDataProvider {
protected:
    Im3dData m_stage;
    Im3dData m_merged;

    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IIm3dDataProvider>(this);
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error ProcessFrameImpl(Time const& time, ExecutionContext const& ec) override {
        // Combine all Im3d data into one list
        ec.m_graph->AddMessageNode([this](JobContext& jc, PortIn<Im3dRenderMessage> input) -> Error {
            input.Handle([this](Im3dRenderMessage const& message) {
                for (uint32_t i = 0; i < message.m_context->getDrawListCount(); ++i) {
                    Im3d::DrawList drawList = message.m_context->getDrawLists()[i];
                    std::copy(drawList.m_vertexData, 
                        drawList.m_vertexData + drawList.m_vertexCount, std::back_inserter(m_stage.m_data));
                    drawList.m_vertexData = nullptr;
                    m_stage.m_drawLists.push_back(drawList);
                }
            });

            return {};
        });
        return {};
    }

    Error MergeImpl(MergeContext const& context) override {
        std::swap(m_stage, m_merged);
        m_stage.Clear();
        return {};
    }

public:
    std::string GetName() const override {
        return "Im3d Handler Module";
    }

    Im3dData const& GetIm3dData() const override {
        return m_merged;
    }
};

std::unique_ptr<EngineModule> Im3dModuleFactory::operator()() {
    return std::make_unique<Im3dModule>();
}