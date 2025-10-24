#pragma once

#include <string_view>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "content.hpp"
#include "entity_manager.hpp"

namespace okami {
    struct SignalExit {};

	struct EngineParams {
		int m_argc = 0;
		const char** m_argv = nullptr;
		std::string_view m_configFilePath = "default.yaml";
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
		void Run(std::optional<size_t> frameCount = std::nullopt);
		void Shutdown();

        entity_t CreateEntity(entity_t parent = kRoot);
        void RemoveEntity(entity_t entity);
        void SetActiveCamera(entity_t e);

        template <typename T>
        void AddComponent(entity_t entity, T component) {
            m_moduleInterface.m_messages.Send(AddComponentSignal<T>{entity, std::move(component)});
        }

        template <typename FactoryT, typename... TArgs>
        auto CreateIOModule(FactoryT factory = FactoryT{}, TArgs&&... args) {
            return m_ioModules.CreateChildFromFactory(factory, std::forward<TArgs>(args)...);
        }

        template <typename FactoryT, typename... TArgs>
        auto CreateUpdateModule(FactoryT factory = FactoryT{}, TArgs&&... args) {
            return m_updateModules.CreateChildFromFactory(factory, std::forward<TArgs>(args)...);
        }

        template <typename FactoryT, typename... TArgs>
        auto CreateRenderModule(FactoryT factory = FactoryT{}, TArgs&&... args) {
            return m_renderModules.CreateChildFromFactory(factory, std::forward<TArgs>(args)...);
        }

        template <typename T>
        T* QueryInterface() const {
            return m_moduleInterface.m_interfaces.Query<T>();
        }

		ModuleInterface& GetModuleInterface() {
            return m_moduleInterface;
        }

        template <ResourceType T>
        ResHandle<T> LoadResource(const std::filesystem::path& path, typename T::LoadParams params = {}) {
            auto* cm = m_moduleInterface.m_interfaces.Query<IContentManager<T>>();
            if (!cm) {
                LogError("No IContentManager<" + std::string(typeid(T).name()) + "> registered in Engine");
                return ResHandle<T>();
            }
            return cm->Load(path, params, m_moduleInterface);
        }

        template <ResourceType T>
        ResHandle<T> CreateResource(T&& data) {
            auto* cm = m_moduleInterface.m_interfaces.Query<IContentManager<T>>();
            if (!cm) {
                throw std::runtime_error("No IContentManager<" + std::string(typeid(T).name()) + "> registered in Engine");
            }
            return cm->Create(std::move(data));
        }

        void AddScript(
            std::function<void(Time const&, ModuleInterface&)> script, 
            std::string_view name = "Unnamed Script");

		std::filesystem::path GetRenderOutputPath(size_t frameIndex);

		Engine(EngineParams params = {});
		~Engine();
	};
}