#pragma once

#include "transform.hpp"
#include "module.hpp"

namespace okami {
    struct OrbitCameraControllerComponent {
        float m_orbitSpeed = 1.0f;
        float m_panSpeed = 1.0f;
        float m_zoomSpeed = 1.0f;
        float m_minDistance = 0.1f;
        float m_maxDistance = 100.0f;
        glm::vec3 m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    };

    struct CameraControllerModuleFactory {
        std::unique_ptr<EngineModule> operator()() const;
    };
}