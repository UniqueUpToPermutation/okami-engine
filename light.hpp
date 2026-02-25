#pragma once

#include "renderer.hpp"

namespace okami {
    struct AmbientLightComponent {
        Color m_color = color::White;
        float m_intensity = 0.1f;
    };

    struct DirectionalLightComponent {
        glm::vec3 m_direction = glm::vec3(-0.5f, -1.0f, -0.5f);
        Color m_color = color::White;
        float m_intensity = 1.0f;
    };

    struct PointLightComponent {
        glm::vec3 m_position = glm::vec3(0.0f);
        Color m_color = color::White;
        float m_intensity = 1.0f;
        float m_range = 10.0f;
    };
}