#include "camera_controllers.hpp"
#include "jobs.hpp"
#include "input.hpp"
#include "camera.hpp"
#include <cmath>

using namespace okami;

class CameraControllerModule final : public EngineModule {
public:
    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
        // Node: handle orbit camera controls (rotate, pan, zoom)
        graph.AddMessageNode([id = GetId(), &registry = params.m_registry](
            JobContext& ctx,
            In<IOState> io,
            In<ScrollMessage> scrollMessages,
            Out<UpdateComponentSignal<Transform>> outTransform,
            Out<UpdateComponentSignal<OrbitCameraControllerComponent>> outOrbit) -> Error {

            float scrollAccum = 0.0f;
            scrollMessages.Handle([&](ScrollMessage const& msg) {
                if (msg.m_captureId == kNoCaptureId) {
                    scrollAccum += static_cast<float>(msg.m_yOffset);
                }
            });

            bool leftDown = false;
            bool middleDown = false;
            double deltaX = 0.0, deltaY = 0.0;
            if (io) {
                leftDown = io->m_mouse.IsButtonPressed(MouseButton::Left);
                middleDown = io->m_mouse.IsButtonPressed(MouseButton::Middle);
                deltaX = io->m_mouse.m_deltaX;
                deltaY = io->m_mouse.m_deltaY;
            }

            registry.view<OrbitCameraControllerComponent const, Transform const, Camera const>().each(
                [&](auto entity,
                    OrbitCameraControllerComponent const& controller,
                    Transform const& transform,
                    Camera const& camera) {

                    // Calculate vector from target to camera
                    glm::vec3 toCamera = transform.m_position - controller.m_target;
                    float distance = glm::length(toCamera);
                    if (distance <= 0.0f) distance = controller.m_minDistance;

                    // spherical coords
                    float azimuth = std::atan2(toCamera.x, toCamera.z);
                    float elevation = std::asin(glm::clamp(toCamera.y / distance, -1.0f, 1.0f));

                    // Handle orbit (left mouse): adjust azimuth/elevation
                    if (leftDown) {
                        const float kOrbitPixToRad = 0.005f; // radians per pixel
                        azimuth -= static_cast<float>(deltaX) * (controller.m_orbitSpeed * kOrbitPixToRad);
                        elevation += static_cast<float>(deltaY) * (controller.m_orbitSpeed * kOrbitPixToRad);
                        const float eps = 0.001f;
                        elevation = glm::clamp(elevation, -glm::half_pi<float>() + eps, glm::half_pi<float>() - eps);
                    }

                    // Handle pan (middle mouse): move target and camera parallel to view plane
                    if (middleDown) {
                        // compute camera right and up from view direction (stable for orbit cameras)
                        glm::vec3 viewDir = glm::normalize(controller.m_target - transform.m_position);
                        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
                        glm::vec3 right = glm::normalize(glm::cross(viewDir, worldUp));
                        if (glm::length(right) < 1e-6f) {
                            // fallback if viewDir is parallel to worldUp
                            right = transform.TransformVector(glm::vec3(1.0f, 0.0f, 0.0f));
                        }
                        glm::vec3 up = glm::normalize(glm::cross(right, viewDir));
                        const float kPanPerPixel = 0.001f; // base world units per pixel
                        float panScale = controller.m_panSpeed * kPanPerPixel * distance; // scale with distance
                        glm::vec3 pan = (-static_cast<float>(deltaX) * right + static_cast<float>(deltaY) * up) * panScale;
                        // compute a local moved target and move the camera by same pan so pan is relative
                        glm::vec3 newTarget = controller.m_target + pan;
                        // persist the new target into the controller component so pan is retained
                        {
                            OrbitCameraControllerComponent updated = controller;
                            updated.m_target = newTarget;
                            outOrbit.Send(UpdateComponentSignal<OrbitCameraControllerComponent>{ entity, updated });
                        }

                        // Move camera position by pan as well
                        glm::vec3 movedCamera = transform.m_position + pan;

                        // Handle scroll zoom as well (after pan): adjust distance between movedCamera and newTarget
                        float currentDistance = glm::length(movedCamera - newTarget);
                        if (currentDistance <= 0.0f) currentDistance = controller.m_minDistance;
                        if (std::abs(scrollAccum) > 0.0f) {
                            float factor = std::exp(-scrollAccum * controller.m_zoomSpeed * 0.1f);
                            float newDistance = currentDistance * factor;
                            newDistance = glm::clamp(newDistance, controller.m_minDistance, controller.m_maxDistance);
                            // move camera along the direction away from target to match new distance
                            glm::vec3 dir = glm::normalize(movedCamera - newTarget);
                            movedCamera = newTarget + dir * newDistance;
                        }

                        Transform newTransform = Transform::LookAt(movedCamera, newTarget, glm::vec3(0.0f, 1.0f, 0.0f));
                        outTransform.Send(UpdateComponentSignal<Transform>{ entity, newTransform });
                        return;
                    }

                    // Handle zoom via scroll
                    if (std::abs(scrollAccum) > 0.0f) {
                        float factor = std::exp(-scrollAccum * controller.m_zoomSpeed * 0.1f);
                        distance = glm::clamp(distance * factor, controller.m_minDistance, controller.m_maxDistance);
                    }

                    // Recompute camera position from spherical coords and target
                    glm::vec3 finalPos;
                    finalPos.x = distance * std::sin(azimuth) * std::cos(elevation);
                    finalPos.y = distance * std::sin(elevation);
                    finalPos.z = distance * std::cos(azimuth) * std::cos(elevation);
                    finalPos += controller.m_target;

                    Transform newTransform = Transform::LookAt(finalPos, controller.m_target, glm::vec3(0.0f, 1.0f, 0.0f));
                    outTransform.Send(UpdateComponentSignal<Transform>{ entity, newTransform });
                }
            );

            return {};
        });

        return {};
    }
};

std::unique_ptr<EngineModule> CameraControllerModuleFactory::operator()() const {
    return std::make_unique<CameraControllerModule>();
}