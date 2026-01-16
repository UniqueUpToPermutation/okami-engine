#pragma once

#include "common.hpp"
#include "entity_manager.hpp"
#include "content.hpp"
#include "texture.hpp"

#include <glm/vec4.hpp>

namespace okami {
	// TODO: This is just a placeholder for testing content loading.
	// We should have a more robust material system in the future
	struct BasicTexturedMaterial {
		ResHandle<Texture> m_colorTexture;
		glm::vec4 m_colorTint{1.0f};
	};

	struct MaterialHandleShared {
		std::atomic<uint32_t> m_refCount{0};
	};

	class IMaterialManagerBase {
	public:
		virtual ~IMaterialManagerBase() = default;

		virtual void DestroyMaterial(MaterialHandleShared* counter) = 0;
	};

	class MaterialHandle {
	private:
		std::type_index m_materialType;
		MaterialHandleShared* m_counter = nullptr;
		IMaterialManagerBase* m_manager = nullptr;

	public:
		inline MaterialHandle() = default;
		inline MaterialHandle(std::type_index materialType, IMaterialManagerBase* manager, MaterialHandleShared* counter) 
			: m_materialType(materialType), m_counter(counter), m_manager(manager) {
			if (m_counter) {
				m_counter->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		inline MaterialHandle(const MaterialHandle& other) 
			:  m_materialType(other.m_materialType), m_counter(other.m_counter), m_manager(other.m_manager) {
			if (m_counter) {
				m_counter->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		inline MaterialHandle& operator=(const MaterialHandle& other) {
			if (this != &other) {
				if (m_counter) {
					m_counter->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				}
				m_materialType = other.m_materialType;
				m_counter = other.m_counter;
				m_manager = other.m_manager;
				if (m_counter) {
					m_counter->m_refCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
			return *this;
		}

		inline ~MaterialHandle() {
			if (m_counter) {
				auto result = m_counter->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				if (result == 1 && m_manager) {
					m_manager->DestroyMaterial(m_counter);
				}
			}
		}

		inline std::type_index GetMaterialType() const {
			return m_materialType;
		}

		inline MaterialHandleShared* Ptr() const {
			return m_counter;
		}
	};

	template <typename T>
	class IMaterialManager : public IMaterialManagerBase {
	public:
		virtual ~IMaterialManager() = default;

		virtual MaterialHandle CreateMaterial(T material) = 0;
	};

	template <typename MaterialT, typename ImplT>
	struct MaterialInfo {
		MaterialT m_material;
		ImplT m_impl;
		std::unique_ptr<MaterialHandleShared> m_counter;
	};

	template <typename MaterialT, typename ImplT>
	class MaterialModuleBase : 
		public EngineModule,
		public IMaterialManager<MaterialT> {
	protected:
		struct CreateMaterialSignal {
			MaterialT m_material;
			std::unique_ptr<MaterialHandleShared> m_counter;
		};

		DefaultSignalHandler<CreateMaterialSignal> m_createMaterialSignalHandler;
		DefaultSignalHandler<MaterialHandleShared*> m_destroyMaterialSignalHandler;

		std::unordered_map<MaterialHandleShared*, MaterialInfo<MaterialT, ImplT>> m_materials;

	public:
		Error RegisterImpl(InterfaceCollection& interfaces) override {
			interfaces.Register<IMaterialManager<MaterialT>>(this);
			return {};
		}

		inline MaterialInfo<MaterialT, ImplT> const* GetMaterialInfo(MaterialHandle const& handle) const {
			if (handle.GetMaterialType() != typeid(MaterialT)) {
				return nullptr;
			}
			auto it = m_materials.find(handle.Ptr());
			if (it == m_materials.end()) {
				return nullptr;
			}
			return &it->second;
		}

		virtual ImplT CreateImpl(MaterialT const& material, std::any userData) = 0;

		MaterialHandle CreateMaterial(MaterialT material) override final {
			auto counter = std::make_unique<MaterialHandleShared>();
			MaterialHandle handle{typeid(MaterialT), this, counter.get()};
			m_createMaterialSignalHandler.Send(CreateMaterialSignal{
				.m_material = std::move(material),
				.m_counter = std::move(counter)
			});
			return handle;
		}

		void DestroyMaterial(MaterialHandleShared* counter) override final {
			m_destroyMaterialSignalHandler.Send(counter);
		}

		void ProcessMaterialSignals(std::any userData) {
			m_createMaterialSignalHandler.Handle([&](CreateMaterialSignal signal) {
				auto impl = CreateImpl(signal.m_material, userData);
				m_materials.emplace(signal.m_counter.get(), MaterialInfo<MaterialT, ImplT>{
					.m_material = std::move(signal.m_material),
					.m_impl = std::move(impl),
					.m_counter = std::move(signal.m_counter)
				});
			});

			m_destroyMaterialSignalHandler.Handle([&](MaterialHandleShared* counter) {
				m_materials.erase(counter);
			});
		}
	};
}