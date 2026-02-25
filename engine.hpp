#pragma once

#include <string_view>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "content.hpp"
#include "entity_manager.hpp"
#include "jobs.hpp"
#include "material.hpp"

namespace okami {
    struct SignalExit {};
    struct MessageExit {};

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

        EngineModule m_modules;

        InterfaceCollection m_interfaces;
        MessageBus m_messages;

        CountSignalHandler<SignalExit> m_exitHandler;

		std::atomic<bool> m_shouldExit{ false };

        IEntityManager* m_entityManager = nullptr;

        entt::registry m_registry;

        InitContext GetInitContext();

	public:
		Error Startup();
		void Run(std::optional<size_t> frameCount = std::nullopt);
		void Shutdown();

        entity_t CreateEntity(entity_t parent = kNullEntity);
        void RemoveEntity(entity_t entity);
        void SetActiveCamera(entity_t e);

        template <typename T>
        void AddComponent(entity_t entity, T component) {
            m_messages.Send(AddComponentSignal<T>{entity, std::move(component)});
        }

        template <typename FactoryT, typename... TArgs>
        auto CreateModule(FactoryT factory = FactoryT{}, TArgs&&... args) {
            return m_modules.CreateChildFromFactory(factory, std::forward<TArgs>(args)...);
        }

        template <typename T>
        T* QueryInterface() const {
            return m_interfaces.Query<T>();
        }

        template <ResourceType T>
        ResHandle<T> LoadResource(const std::filesystem::path& path, typename T::LoadParams params = {}) {
            auto* cm = m_interfaces.Query<IContentManager<T>>();
            if (!cm) {
                OKAMI_LOG_ERROR("No IContentManager<" + std::string(typeid(T).name()) + "> registered in Engine");
                return ResHandle<T>();
            }
            return cm->Load(path, params, m_interfaces);
        }

        template <ResourceType T>
        ResHandle<T> CreateResource(T&& data) {
            auto* cm = m_interfaces.Query<IContentManager<T>>();
            if (!cm) {
                OKAMI_LOG_ERROR("No IContentManager<" + std::string(typeid(T).name()) + "> registered in Engine");
                return ResHandle<T>();
            }
            return cm->Create(std::move(data));
        }

        template <typename T>
        MaterialHandle CreateMaterial(T material) {
            auto* mm = m_interfaces.Query<IMaterialManager<T>>();
            if (!mm) {
                OKAMI_LOG_ERROR("No IMaterialManager<" + std::string(typeid(T).name()) + "> registered in Engine");
                return MaterialHandle();
            }
            return mm->CreateMaterial(std::move(material));
        }

        void AddScriptBundle(
            std::function<void(JobGraph&, BuildGraphParams const&)> script, 
            std::string_view name = "Unnamed Script");

        template <typename Callable>
        void AddScript(Callable task, std::string_view name = "Unnamed Script") {
            AddScriptBundle(
                [task = std::move(task)](JobGraph& graph, BuildGraphParams const& params) {
                    graph.AddMessageNode(std::move(task));
                },
                name);
        }

        entt::registry const& GetRegistry() const { return m_registry; }

		std::filesystem::path GetRenderOutputPath(size_t frameIndex);

		Engine(EngineParams params = {});
		~Engine();
	};
}