#pragma once

#include "../aabb.hpp"

namespace okami {

    template <typename T>
    concept Collider2D = requires(T t) {
        { t.GetAABB() } -> std::convertible_to<AABB2>;
    };

    struct ColliderBox2D {
        glm::vec2 m_size;
        glm::vec2 m_offset;

        AABB2 GetAABB() const {
            return AABB2{
                m_offset - m_size * 0.5f,
                m_offset + m_size * 0.5f
            };
        }
    };

    struct ColliderCircle2D {
        float m_radius;
        glm::vec2 m_offset;

        AABB2 GetAABB() const {
            return AABB2{
                m_offset - glm::vec2(m_radius),
                m_offset + glm::vec2(m_radius)
            };
        }
    };

    static_assert(Collider2D<ColliderBox2D>, "ColliderBox2D does not satisfy Collider2D concept");
    
    class RigidBody2D {

    };
    
    class PhysicsWorld2D {

    };
}