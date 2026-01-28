#pragma once

#include "common.hpp"
#include "entity_manager.hpp"
#include "content.hpp"
#include "texture.hpp"

#include <glm/vec4.hpp>

namespace okami {
	struct DefaultMaterial {
	};

	// TODO: This is just a placeholder for testing content loading.
	// We should have a more robust material system in the future
	struct BasicTexturedMaterial {
		ResHandle<Texture> m_colorTexture;
		glm::vec4 m_colorTint{1.0f};
	};

	struct MaterialHandleShared {
		std::type_index m_materialType = typeid(BasicTexturedMaterial);
		std::atomic<uint32_t> m_refCount{0};
		entity_t m_entity = kNullEntity;
	};

	class IMaterialManagerBase {
	public:
		virtual ~IMaterialManagerBase() = default;

		virtual void DestroyMaterial(entity_t entity) = 0;
	};

	class MaterialHandle {
	private:
		MaterialHandleShared* m_counter = nullptr;
		IMaterialManagerBase* m_manager = nullptr;

	public:
		inline MaterialHandle() = default;
		inline MaterialHandle(IMaterialManagerBase* manager, MaterialHandleShared* counter) 
			: m_counter(counter), m_manager(manager) {
			if (m_counter) {
				m_counter->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		inline MaterialHandle(const MaterialHandle& other) 
			: m_counter(other.m_counter), m_manager(other.m_manager) {
			if (m_counter) {
				m_counter->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		inline MaterialHandle& operator=(const MaterialHandle& other) {
			if (this != &other) {
				if (m_counter) {
					m_counter->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				}
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
					m_manager->DestroyMaterial(m_counter->m_entity);
				}
			}
		}

		inline std::type_index GetMaterialType() const {
			return m_counter ? m_counter->m_materialType : std::type_index(typeid(void));
		}

		inline MaterialHandleShared* Ptr() const {
			return m_counter;
		}

		inline entity_t GetEntity() const {
			return m_counter ? m_counter->m_entity : kNullEntity;
		}

		inline operator bool() const {
			return m_counter != nullptr;
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
		using shared_component_t = std::unique_ptr<MaterialHandleShared>;

		struct CreateMaterialSignal {
			MaterialT m_material;
			shared_component_t m_counter;
		};

		DefaultSignalHandler<CreateMaterialSignal> m_createMaterialSignalHandler;
		DefaultSignalHandler<entity_t> m_destroyMaterialSignalHandler;

		entt::registry const* m_registry = nullptr;
		IEntityManager* m_entityManager = nullptr;

		template<bool HasImpl>
		void DestroyEntity(RecieveMessagesParams const& params, entity_t entity) {
			if constexpr (HasImpl) {
				auto it = params.m_registry.template try_get<ImplT>(entity);
				if (it) {
					auto& impl = (*it)->m_impl;
					DestroyImpl(impl);
				}
			}
			params.m_registry.destroy(entity);
		}

	public:
		Error RegisterImpl(InterfaceCollection& interfaces) override {
			interfaces.Register<IMaterialManager<MaterialT>>(this);
			return {};
		}

		Error StartupImpl(InitContext const& context) override {
			m_registry = &context.m_registry;
			m_entityManager = context.m_interfaces.Query<IEntityManager>();
			OKAMI_ERROR_RETURN_IF(m_entityManager == nullptr, "MaterialModule requires an IEntityManager to function");	
			return {};
		}

		inline ImplT const* GetMaterialImpl(entity_t entity) const {
			return m_registry->template try_get<ImplT const>(entity);
		}

		inline MaterialT const* GetMaterialInfo(entity_t entity) const {
			return m_registry->template try_get<MaterialT const>(entity);
		}

		virtual ImplT CreateImpl(MaterialT const& material) = 0;
		virtual void DestroyImpl(ImplT& impl) = 0;

		MaterialHandle CreateMaterial(MaterialT material) override final {
			auto entity = m_entityManager->CreateEntity();
			auto counter = std::make_unique<MaterialHandleShared>();
			counter->m_entity = entity;
			counter->m_materialType = typeid(MaterialT);

			MaterialHandle handle{this, counter.get()};
			m_createMaterialSignalHandler.Send(CreateMaterialSignal{
				.m_material = std::move(material),
				.m_counter = std::move(counter)
			});
			return handle;
		}

		void DestroyMaterial(entity_t entity) override final {
			m_destroyMaterialSignalHandler.Send(entity);
		}

		Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
			m_createMaterialSignalHandler.Handle([&](CreateMaterialSignal msg) {
				auto impl = CreateImpl(msg.m_material);
				auto entity = msg.m_counter->m_entity;
				params.m_registry.template emplace<shared_component_t>(entity, std::move(msg.m_counter));
				params.m_registry.template emplace<ImplT>(entity, std::move(impl));
				params.m_registry.template emplace<MaterialT>(entity, std::move(msg.m_material));
			});

			m_destroyMaterialSignalHandler.Handle([&](entity_t entity) {
				DestroyEntity<!std::is_empty_v<ImplT>>(params, entity);
			});

			return {};
		}
	};
}