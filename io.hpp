#pragma once

#include "content.hpp"
#include "module.hpp"

namespace okami {
    template <ResourceType T> 
    class IOModule : public EngineModule {
    private:
        std::shared_ptr<IMessageQueue<LoadResourceMessage<T>>> m_load_queue;

    protected:
        virtual OnResourceLoadedMessage<T> LoadResource(LoadResourceMessage<T>&& msg) = 0;

        Error RegisterImpl(ModuleInterface& mi) override {
            m_load_queue = mi.m_messages.CreateQueue<LoadResourceMessage<T>>();
            return {};
        }
        Error StartupImpl(ModuleInterface&) override {
            return {};
        }
        void ShutdownImpl(ModuleInterface&) override {
        }

        Error ProcessFrameImpl(Time const&, ModuleInterface& mi) override {
            while (auto msg = m_load_queue->GetMessage()) {
                auto result = LoadResource(std::move(*msg));
                mi.m_messages.Send(std::move(result));
            }

            return {};
        }

        Error MergeImpl() override {
            return {};
        }

    public:
        std::string_view GetName() const override {
            return "IO Module";
        }
    };

    struct TextureIOModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}