#pragma once

#include "renderer.hpp"
#include "camera.hpp"
#include "transform.hpp"

#include <glm/vec2.hpp>

namespace okami {
    struct AmbientLightComponent {
        Color m_color = color::White;
        float m_intensity = 0.1f;
    };

    struct DirectionalLightComponent {
        glm::vec3 m_direction = glm::vec3(-0.5f, -1.0f, -0.5f);
        Color m_color = color::White;
        float m_intensity = 1.0f;
        bool  b_castShadow = false;
    };

    struct PointLightComponent {
        glm::vec3 m_position = glm::vec3(0.0f);
        Color m_color = color::White;
        float m_intensity = 1.0f;
        float m_range = 10.0f;
    };

    // Describes the orthographic camera and world-space transform for a single
    // shadow map cascade computed from a directional light.
    struct ShadowCascade {
        Camera    camera;
        Transform transform;
    };

    // Computes a shadow cascade that tightly fits the portion of the view
    // frustum up to shadowDistance world units from the player camera.
    // shadowBehind controls how far behind the frustum the light camera
    // is pulled back to capture occluders outside the view frustum.
    ShadowCascade ComputeShadowCascade(
        DirectionalLightComponent const& light,
        Camera                    const& viewCamera,
        Transform                 const& viewTransform,
        glm::ivec2                const& viewportSize,
        float                            shadowDistance,
        int                              shadowMapSize,
        float                            shadowBehind = 100.0f);
}