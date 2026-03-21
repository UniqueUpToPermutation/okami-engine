#pragma once

#include "common.hpp"
#include "module.hpp"

#include <memory>
#include <cstdint>

#include <entt/entity/entity.hpp>

namespace std {
	inline std::string to_string(entt::entity e) {
		return std::to_string(entt::to_integral(e));
	}
}

namespace okami
{
	class ISignalBus;
	class Engine;

	using entity_t = entt::entity;
	constexpr entity_t kNullEntity = entt::null;
	
	struct NameComponent {
		std::string m_name;
	};

	struct EntityRemoveMessage {
		entity_t m_entity = kNullEntity;
	};

	struct EntityCreatedSignal {
		entity_t m_entity = kNullEntity;
	};

	struct EntityParentChangeSignal {
		entity_t m_entity = kNullEntity;
		entity_t m_newParent = kNullEntity;
	};

	template <typename T>
	struct AddComponentSignal {
		entity_t m_entity = kNullEntity;
		T m_component;
	};

	template <typename T>
	struct UpdateComponentSignal {
		entity_t m_entity = kNullEntity;
		T m_component;
	};

	template <typename T>
	struct RemoveComponentSignal {
		entity_t m_entity = kNullEntity;
	};

	// Signals for registry context variables (singleton per type, no entity).
	template <typename T>
	struct AddCtxSignal {
		T m_value;
	};

	template <typename T>
	struct UpdateCtxSignal {
		T m_value;
	};

	template <typename T>
	struct RemoveCtxSignal {};

	struct EntityTreeComponent {
		entity_t m_parent = kNullEntity;
		entity_t m_firstChild = kNullEntity;
		entity_t m_nextSibling = kNullEntity;
		entity_t m_prevSibling = kNullEntity;
		entity_t m_lastChild = kNullEntity;
	};

	struct EntityManagerCtx {
		entity_t m_rootEntity = kNullEntity;
	};

    class IEntityManager {
	private:
		virtual entity_t CreateEntity() = 0;

    public:
		inline entity_t CreateEntity(
			Out<EntityCreatedSignal> createdPort,
			Out<EntityParentChangeSignal> parentPort, 
			entity_t parent = kNullEntity) {
			auto entity = CreateEntity();
			createdPort.Send(EntityCreatedSignal{entity});
			parentPort.Send(EntityParentChangeSignal{entity, parent});
			return entity;
		}

        inline void RemoveEntity(Out<EntityRemoveMessage> port, entity_t entity) {
			port.Send(EntityRemoveMessage{entity});
		}

        virtual ~IEntityManager() = default;
    };

    struct EntityManagerFactory {
        std::unique_ptr<EngineModule> operator()(entt::registry&);
    };
}