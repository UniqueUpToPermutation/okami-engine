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

    // First-person camera controller.
    //
    // Hold the right mouse button and drag to look around (yaw/pitch).
    // Use W/A/S/D to move forward/left/backward/right in the horizontal plane.
    // Use Q/E to move down/up along the world Y axis.
    //
    // Yaw and pitch are derived from the camera's Transform rotation each frame
    // rather than being stored on the component, so no per-frame write-back
    // to the component is needed.
    struct FirstPersonCameraControllerComponent {
        float m_moveSpeed       = 5.0f;    // world units per second
        float m_lookSensitivity = 0.002f;  // radians per cursor pixel
    };

    struct CameraControllerModuleFactory {
        std::unique_ptr<EngineModule> operator()() const;
    };
}