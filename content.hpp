#pragma once

#include "common.hpp"
#include "module.hpp"
#include "paths.hpp"
#include "entity_manager.hpp"

#include <filesystem>

namespace okami {
	template <typename T>
	concept ResourceType = requires {
		typename T::Desc;
		typename T::LoadParams;
	};

	template <ResourceType T>
	struct Resource {
		typename T::Desc m_desc;
		std::filesystem::path m_path;
		entity_t m_entity = kNullEntity;
		std::atomic<bool> m_loaded{ false };
		std::atomic<int> m_refCount{ 0 };
	};

	class IResourceDestroyer {
	public:
		virtual ~IResourceDestroyer() = default;

		virtual void DestroyResource(entity_t id) = 0;
	};

    template <ResourceType T>
	struct ResHandle {
	private:
		Resource<T>* m_resource = nullptr;
		IResourceDestroyer* m_destroyer = nullptr;

	public:
		using desc_t = typename T::Desc;

		inline Resource<T>* Ptr() const {
			return m_resource;
		}

		inline ResHandle() = default;
		inline ResHandle(
			Resource<T>* resource, IResourceDestroyer* destroyer) : m_resource(resource), m_destroyer(destroyer) {
			if (m_resource) {
				m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline ResHandle(const ResHandle& other) : m_resource(other.m_resource), m_destroyer(other.m_destroyer) {
			if (m_resource) {
				m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline ResHandle& operator=(const ResHandle& other) {
			if (this != &other) {
				if (m_resource) {
					m_resource->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				}
				m_resource = other.m_resource;
				m_destroyer = other.m_destroyer;
				if (m_resource) {
					m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
			return *this;
		}
		inline ~ResHandle() {
			if (m_resource) {
				auto count = m_resource->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				if (count == 1 && m_destroyer) {
					m_destroyer->DestroyResource(m_resource->m_entity);
				}
			}
		}
		desc_t const& operator*() const {
			if (!m_resource || !m_resource->m_loaded.load(std::memory_order_acquire)) {
				throw std::runtime_error("Resource not loaded");
			}
			return m_resource->m_desc;
		}
		desc_t const* operator->() const {
			if (!m_resource || !m_resource->m_loaded.load(std::memory_order_acquire)) {
				throw std::runtime_error("Resource not loaded");
			}
			return &m_resource->m_desc;
		}
		inline desc_t const& GetDesc() const {
			return m_resource->m_desc;
		}
		inline bool IsLoaded() const {
			return m_resource && m_resource->m_loaded.load(std::memory_order_acquire);
		}
		inline std::string_view GetPath() const {
			return m_resource ? m_resource->m_path : std::string_view();
		}
		inline operator bool() const {
			return IsLoaded();
		}
		inline entity_t GetEntity() const {
			return m_resource->m_entity;
		}
	};

    template <ResourceType T>
    class IContentManager {
    public:
		// Both of these should be thread-safe
        virtual ResHandle<T> Load(
			const std::filesystem::path& path,
			typename T::LoadParams params,
			InterfaceCollection& mi) = 0;
        virtual ResHandle<T> Create(T&& data) = 0;
    };

	template <ResourceType T>
	struct LoadResourceSignal {
		std::filesystem::path m_path;
		typename T::LoadParams m_params;
		ResHandle<T> m_handle;
	};

	template <typename T>
	struct OnResourceLoadedEvent {
		Expected<T> m_data;
		ResHandle<T> m_handle;
	};

	template <
		ResourceType T, 
		typename TImpl
	>
	struct ResourceImplPair {
		Resource<T> m_resource;
		TImpl m_impl;
	};

    template <
		ResourceType T, 
		typename TImpl
	>
    class ContentModule : 
		public IContentManager<T>,
		public IResourceDestroyer,
		public EngineModule {
    private:
		// Maps a file path to a resource pointer
		// Protected by a mutex because it may be accessed from multiple threads
        std::mutex m_path_mtx;
        std::unordered_map<std::filesystem::path, entity_t, PathHash> m_path_to_res;

		// Consumes messages regarding newly loaded resources
		// These are sent by the IO thread
		DefaultSignalHandler<OnResourceLoadedEvent<T>> m_loaded_handler;

		// Consumes messages regarding new resources to be created
		DefaultSignalHandler<std::unique_ptr<ResourceImplPair<T, TImpl>>> m_new_resources;

		DefaultSignalHandler<entity_t> m_destroy_resource_handler;

		IEntityManager* m_entityManager = nullptr;
		entt::registry const* m_registry = nullptr;

	protected:
		virtual Expected<std::pair<typename T::Desc, TImpl>> CreateResource(T&& data) = 0;
		virtual void DestroyResourceImpl(TImpl& impl) = 0;

		Error RegisterImpl(InterfaceCollection& ic) override {
			ic.Register<IContentManager<T>>(this);
			ic.RegisterSignalHandler<OnResourceLoadedEvent<T>>(&m_loaded_handler);
			return {};
		}

        Error StartupImpl(InitContext const& ic) override {
			m_registry = &ic.m_registry;
			m_entityManager = ic.m_interfaces.Query<IEntityManager>();
			OKAMI_ERROR_RETURN_IF(m_entityManager == nullptr, "ContentModule requires an IEntityManager to function");
			return {};
		}
		
        void ShutdownImpl(InitContext const& ic) override {
			{
				std::lock_guard<std::mutex> lock(m_path_mtx);
				m_path_to_res.clear();
			}

			m_loaded_handler.Clear();
			m_new_resources.Clear();
		}

	public:
		using component_t = std::unique_ptr<ResourceImplPair<T, TImpl>>;

		// Protected method to access implementation for derived classes
		TImpl* GetImpl(const ResHandle<T>& handle) const {
			auto implPair = m_registry->template try_get<component_t>(handle.GetEntity());
			if (implPair) {
				return &(*implPair)->m_impl;
			} else {
				return nullptr;
			}
		}

		void DestroyResource(entity_t id) override {
			m_destroy_resource_handler.Send(id);
		}

		Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
			Error e;

			// Move everything from the new resources queue to the Resource* -> Impl map
			m_new_resources.Handle([&](component_t msg) {
				params.m_registry.template emplace<component_t>(msg.get()->m_resource.m_entity, std::move(msg));
			});

			// Process all just loaded resources
			m_loaded_handler.Handle([&](OnResourceLoadedEvent<T> msg) {
				if (!msg.m_data) {
					e += msg.m_data.error();
					return;
				}

				auto result = CreateResource(std::move(*msg.m_data));
				if (!result) {
					e += result.error();
					return;
				}

				auto* implPair = params.m_registry.template try_get<component_t>(msg.m_handle.GetEntity());
				if (!implPair) {
					e += OKAMI_ERROR("Loaded resource entity not found in registry");
					return;
				}

				implPair->get()->m_resource.m_desc = std::move(result->first);
				implPair->get()->m_impl = std::move(result->second);
				implPair->get()->m_resource.m_loaded.store(true, std::memory_order_release);
			});

			// Process all just destroyed resources
			m_destroy_resource_handler.Handle([&](entity_t entity) {
				auto* implPair = params.m_registry.template try_get<component_t>(entity);

				if (!implPair) {
					e += OKAMI_ERROR("Trying to destroy resource with entity that not found in registry");
					return;
				}

				std::filesystem::path path = implPair->get()->m_resource.m_path;
				if (!path.empty()) {
					std::lock_guard<std::mutex> lock(m_path_mtx);
					if (implPair->get()->m_resource.m_refCount != 0) {
						return; // Resource still has references, don't destroy yet
					}
					m_path_to_res.erase(path);
				}

				// Ask implementation to destroy the resource
				DestroyResourceImpl(implPair->get()->m_impl);
				params.m_registry.template erase<component_t>(entity);
			});

			return e;
		}

		ResHandle<T> Load(
			const std::filesystem::path& path,
			typename T::LoadParams params,
			InterfaceCollection& ic) override {
	
			std::unique_ptr<ResourceImplPair<T, TImpl>> impl;
			entity_t entity = kNullEntity;

			{
				std::lock_guard<std::mutex> lock(m_path_mtx);
				auto it = m_path_to_res.find(path);
				if (it != m_path_to_res.end()) {
					auto* implPair = m_registry->template try_get<component_t>(it->second);
					return ResHandle<T>(&implPair->get()->m_resource, this);
				}
				else {
					// Create a new resource with its implementation
					entity = m_entityManager->CreateEntity();
					impl = std::make_unique<ResourceImplPair<T, TImpl>>();
					m_path_to_res.emplace_hint(it, path, impl->m_resource.m_entity);
				}
			}
			
			// Set resource path
			impl->m_resource.m_path = path;
			impl->m_resource.m_entity = entity;

			auto res = ResHandle<T>(&impl->m_resource, this);

			// Ask IO thread to load the resource data
			ic.SendSignal(LoadResourceSignal<T>{
				.m_path = path,
				.m_params = std::move(params),
				.m_handle = res,
			});
			m_new_resources.Send(std::move(impl));
			
			return res;
		}

        ResHandle<T> Create(T&& data) override {
			// Create a new resource with its implementation
			auto entity = m_entityManager->CreateEntity();
			auto impl = std::make_unique<ResourceImplPair<T, TImpl>>();

			impl->m_resource.m_entity = entity;

			auto res = ResHandle<T>(&impl->m_resource, this);

			m_new_resources.Send(std::move(impl));
			m_loaded_handler.Send(OnResourceLoadedEvent<T>{
				.m_data = std::move(data),
				.m_handle = res,
			});
			return res;
		}

		std::string GetName() const override {
			auto typeName = typeid(T).name();
			return "Content Module <" + std::string{typeName} + ">";
		}
    };
}