#pragma once

#include "content.hpp"
#include "module.hpp"

namespace okami {
    template <ResourceType T> 
    class IOModule : public EngineModule, public IIOModule {
    private:
        DefaultSignalHandler<LoadResourceSignal<T>> m_load_handler;

    protected:
        virtual OnResourceLoadedSignal<T> LoadResource(LoadResourceSignal<T>&& msg) = 0;

        Error RegisterImpl(InterfaceCollection& interfaces) override {
            interfaces.RegisterSignalHandler<LoadResourceSignal<T>>(&m_load_handler);
            interfaces.Register<IIOModule>(this);
            
            return {};
        }

        Error IOProcess(InterfaceCollection& interfaces) override {
            m_load_handler.Handle([this, &interfaces](LoadResourceSignal<T> const& msg) {
                auto result = LoadResource(LoadResourceSignal<T>{msg});
                interfaces.SendSignal(std::move(result));
            });

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