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

	struct EntityRemoveMessage {
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

    class IEntityManager {
    public:
        virtual entity_t CreateEntity() = 0;

		inline entity_t CreateEntity(Out<EntityParentChangeSignal> port, entity_t parent) {
			auto entity = CreateEntity();
			if (parent != kNullEntity) {
				port.Send(EntityParentChangeSignal{entity, parent});
			}
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