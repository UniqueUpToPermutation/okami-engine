#pragma once

#include "common.hpp"
#include "module.hpp"
#include "paths.hpp"

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
		std::string m_path;
		std::atomic<bool> m_loaded{ false };
		std::atomic<int> m_refCount{ 0 };
	};

    template <ResourceType T>
	struct ResHandle {
	private:
		Resource<T>* m_resource = nullptr;

	public:
		using desc_t = typename T::Desc;

		inline Resource<T>* Ptr() const {
			return m_resource;
		}

		inline ResHandle() = default;
		inline ResHandle(Resource<T>* resource) : m_resource(resource) {
			if (m_resource) {
				m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline ResHandle(const ResHandle& other) : m_resource(other.m_resource) {
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
				if (m_resource) {
					m_resource->m_refCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
			return *this;
		}
		inline ~ResHandle() {
			if (m_resource) {
				m_resource->m_refCount.fetch_sub(1, std::memory_order_relaxed);
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
	struct OnResourceLoadedSignal {
		Expected<T> m_data;
		ResHandle<T> m_handle;
	};

    template <
		ResourceType T, 
		typename TImpl>
    class ContentModule : public IContentManager<T>, public EngineModule {
    private:
        struct ImplPair {
            Resource<T> m_resource;
            TImpl m_impl;
        };

		// Maps a file path to a resource pointer
		// Protected by a mutex because it may be accessed from multiple threads
        std::mutex m_path_mtx;
        std::unordered_map<std::filesystem::path, Resource<T>*, PathHash> m_path_to_res;

		// Maps a resource pointer to its implementation
		// Only accessed from the main thread
        std::unordered_map<Resource<T>*, std::unique_ptr<ImplPair>> m_res_to_impl;


		// Consumes messages regarding newly loaded resources
		// These are sent by the IO thread
		DefaultSignalHandler<OnResourceLoadedSignal<T>> m_loaded_handler;

		// Consumes messages regarding new resources to be created
		DefaultSignalHandler<std::unique_ptr<ImplPair>> m_new_resources;

	protected:
		virtual Expected<std::pair<typename T::Desc, TImpl>> CreateResource(T&& data, std::any userData) = 0;

		Error RegisterImpl(InterfaceCollection& ic) override {
			ic.Register<IContentManager<T>>(this);
			ic.RegisterSignalHandler<OnResourceLoadedSignal<T>>(&m_loaded_handler);
			
			return {};
		}

        Error StartupImpl(InitContext const& ic) override {
			return {};
		}
		
        void ShutdownImpl(InitContext const& ic) override {
			{
				std::lock_guard<std::mutex> lock(m_path_mtx);
				m_path_to_res.clear();
			}

			m_res_to_impl.clear();
			m_loaded_handler.Clear();
			m_new_resources.Clear();
		}

	public:
		// Protected method to access implementation for derived classes
		TImpl* GetImpl(const ResHandle<T>& handle) {
			if (!handle.IsLoaded()) {
				return nullptr;
			}
			auto it = m_res_to_impl.find(handle.Ptr());
			if (it != m_res_to_impl.end()) {
				return &it->second->m_impl;
			}
			return nullptr;
		}

		Error ProcessNewResources(std::any userData) {
			Error e;

			// Move everything from the new resources queue to the Resource* -> Impl map
			m_new_resources.Handle([this](std::unique_ptr<ImplPair> msg) {
				auto implIt = m_res_to_impl.find(&msg->m_resource);
				if (implIt == m_res_to_impl.end()) {
					// New resource, add to map
					m_res_to_impl.emplace_hint(implIt, &msg->m_resource, std::move(msg));
				}
			});

			// Process all just loaded resources
			m_loaded_handler.Handle([this, &e, &userData](OnResourceLoadedSignal<T> msg) {
				if (!msg.m_data) {
					e += msg.m_data.error();
					return;
				}

				auto implIt = m_res_to_impl.find(msg.m_handle.Ptr());
				if (implIt == m_res_to_impl.end()) {
					e += OKAMI_ERROR("Loaded resource not found in implementation map");
					return;
				}

				auto& impl = implIt->second;

				auto result = CreateResource(std::move(*msg.m_data), userData);
				if (!result) {
					e += result.error();
					return;
				}

				impl->m_impl = std::move(result->second);
				msg.m_handle.Ptr()->m_desc = std::move(result->first);
				msg.m_handle.Ptr()->m_loaded.store(true, std::memory_order_release);
			});

			return e;
		}

		ResHandle<T> Load(
			const std::filesystem::path& path,
			typename T::LoadParams params,
			InterfaceCollection& ic) override {
			
			std::unique_ptr<ImplPair> impl;

			{
				std::lock_guard<std::mutex> lock(m_path_mtx);
				auto it = m_path_to_res.find(path);
				if (it != m_path_to_res.end()) {
					return ResHandle<T>(it->second);
				}
				else {
					// Create a new resource with its implementation
					impl = std::make_unique<ImplPair>();
					m_path_to_res.emplace_hint(it, path, &impl->m_resource);
				}
			}
			
			// Set resource path
			impl->m_resource.m_path = path.string();
			auto res = ResHandle<T>(&impl->m_resource);

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
			auto impl = std::make_unique<ImplPair>();
			auto res = ResHandle<T>(&impl->m_resource);
			m_new_resources.Send(std::move(impl));
			m_loaded_handler.Send(OnResourceLoadedSignal<T>{
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