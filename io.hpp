#pragma once

#include "content.hpp"
#include "module.hpp"

namespace okami {
    template <ResourceType T> 
    class IOModule : public EngineModule {
    private:
        DefaultSignalHandler<LoadResourceSignal<T>> m_load_handler;

    protected:
        virtual OnResourceLoadedSignal<T> LoadResource(LoadResourceSignal<T>&& msg) = 0;

        Error RegisterImpl(ModuleInterface& mi) override {
            mi.m_interfaces.RegisterSignalHandler<LoadResourceSignal<T>>(&m_load_handler);
            return {};
        }
        Error StartupImpl(ModuleInterface&) override {
            return {};
        }
        void ShutdownImpl(ModuleInterface&) override {
        }

        Error ProcessFrameImpl(Time const&, ModuleInterface& mi) override {
            m_load_handler.Handle([this, &mi](LoadResourceSignal<T> const& msg) {
                auto result = LoadResource(LoadResourceSignal<T>{msg});
                mi.m_interfaces.SendSignal(std::move(result));
            });

            return {};
        }

        Error MergeImpl(ModuleInterface& a) override {
            return {};
        }

    public:
        std::string GetName() const override {
            auto typeName = typeid(T).name();
            return "IO Module <" + std::string{typeName} + ">";
        }
    };

    struct TextureIOModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };

    struct GeometryIOModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}