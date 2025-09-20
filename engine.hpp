#pragma once

#include <string_view>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "entity_manager.hpp"

namespace okami {
    struct SignalExit {};

	struct EngineParams {
		int m_argc = 0;
		const char** m_argv = nullptr;
		std::string_view m_configFilePath = "default.yaml";
		bool m_headlessMode = false;
		std::string_view m_headlessOutputFileStem = "output";
		bool m_forceLogToConsole = false;
	};

    class IEntityManager;

    class Engine final {
	private:
		EngineParams m_params;

        EmptyModule m_ioModules;
        EmptyModule m_updateModules;
        EmptyModule m_renderModules;

        ModuleInterface m_moduleInterface;

        std::shared_ptr<IMessageQueue<SignalExit>> m_exitHandler = nullptr;

		std::atomic<bool> m_shouldExit{ false };

        IEntityManager* m_entityManager = nullptr;

	public:
		Error Startup();
		void UploadResources();
		void Run(std::optional<size_t> frameCount = std::nullopt);
		void Shutdown();

        entity_t CreateEntity(entity_t parent = kRoot);
        void RemoveEntity(entity_t entity);
        void SetActiveCamera(entity_t e);

        template <typename T>
        void AddComponent(entity_t entity, T component) {
            m_moduleInterface.m_messages.SendMessage(AddComponentSignal<T>{entity, std::move(component)});
        }

        template <typename FactoryT>
        auto CreateIOModule(FactoryT factory = FactoryT{}) {
            return m_ioModules.CreateChildFromFactory(factory);
        }

        template <typename FactoryT>
        auto CreateUpdateModule(FactoryT factory = FactoryT{}) {
            return m_updateModules.CreateChildFromFactory(factory);
        }

        template <typename FactoryT>
        auto CreateRenderModule(FactoryT factory = FactoryT{}) {
            return m_renderModules.CreateChildFromFactory(factory);
        }

		ModuleInterface& GetModuleInterface() {
            return m_moduleInterface;
        }

        void AddScript(
            std::function<void(Time const&, ModuleInterface&)> script, 
            std::string_view name = "Unnamed Script");

		std::filesystem::path GetRenderOutputPath(size_t frameIndex);

		Engine(EngineParams params = {});
		~Engine();
	};
}