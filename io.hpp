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

        Error RegisterImpl(InterfaceCollection& interfaces) override {
            interfaces.RegisterSignalHandler<LoadResourceSignal<T>>(&m_load_handler);
            return {};
        }
        Error StartupImpl(InitContext const&) override {
            return {};
        }
        void ShutdownImpl(InitContext const&) override {
        }

        Error ProcessFrameImpl(Time const&, ExecutionContext const& a) override {
            m_load_handler.Handle([this, &a](LoadResourceSignal<T> const& msg) {
                auto result = LoadResource(LoadResourceSignal<T>{msg});
                a.m_interfaces.SendSignal(std::move(result));
            });

            return {};
        }

        Error MergeImpl(MergeContext const& a) override {
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