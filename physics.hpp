#pragma once

#include "module.hpp"
#include "entity_manager.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace okami {
    struct RigidBody2D {
        enum class Type {
            Dynamic,
            Kinematic,
            Static
        };

        float m_mass = 1.0f;
        float m_friction = 0.5f;
        float m_restitution = 0.0f;
        Type m_type = Type::Static;
    };

    struct BoxCollider2D {
        glm::vec2 m_size = glm::vec2(1.0f, 1.0f);
        glm::vec2 m_offset = glm::vec2(0.0f, 0.0f);
        size_t m_colliderMask = 0xFFFFFFFF;
        size_t m_collidesWithMask = 0xFFFFFFFF;
    };

    struct AddVelocityMessage {
        entity_t m_entity;
        glm::vec3 m_velocity = glm::vec3(0.0f);
        glm::vec3 m_angular = glm::vec3(0.0f);
    };

    struct AddVelocity2DMessage {
        entity_t m_entity;
        glm::vec2 m_velocity = glm::vec2(0.0f);
        float m_angular = 0.0f;
    };

    struct AddForce2DMessage {
        entity_t m_entity;
        glm::vec2 m_force = glm::vec2(0.0f);
        float m_torque = 0.0f;
    };

    struct PhysicsModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}