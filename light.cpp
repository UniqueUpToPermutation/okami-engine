#include "light.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>

using namespace okami;

ShadowCascade okami::ComputeShadowCascade(
    DirectionalLightComponent const& light,
    Camera                    const& viewCamera,
    Transform                 const& viewTransform,
    glm::ivec2                const& viewportSize,
    float                            zNear,
    float                            zFar,
    int                              shadowMapSize,
    float                            shadowBehind)
{
    // ── 1. Extract projection parameters ────────────────────────────────────
    // zNear / zFar define the view-space slice for this cascade; clamp them
    // to the camera's own near/far so we never go outside its frustum.
    float nearZ = zNear, farZ = zFar;
    float tanHalfFovY = 1.0f, aspect = 1.0f;
    bool  isPerspective = false;

    std::visit([&](auto const& proj) {
        using T = std::decay_t<decltype(proj)>;
        if constexpr (std::is_same_v<T, PerspectiveProjection>) {
            nearZ        = std::max(zNear, proj.m_nearZ);
            farZ         = std::min(zFar,  proj.m_farZ);
            tanHalfFovY  = std::tan(proj.m_fovY * 0.5f);
            aspect       = proj.m_aspectRatio.value_or(
                static_cast<float>(viewportSize.x) / static_cast<float>(viewportSize.y));
            isPerspective = true;
        } else if constexpr (std::is_same_v<T, OrthographicProjection>) {
            nearZ = std::max(zNear, proj.m_nearZ);
            farZ  = std::min(zFar,  proj.m_farZ);
        }
    }, viewCamera.m_projection);

    // ── 2. Build 8 frustum corners in view space ────────────────────────────
    std::array<glm::vec3, 8> corners;
    if (isPerspective) {
        const float tanHalfFovX = tanHalfFovY * aspect;
        for (int i = 0; i < 8; ++i) {
            const float z    = (i < 4) ? -nearZ : -farZ;  // OpenGL view space: -Z forward
            const float dist = std::abs(z);
            const float x    = ((i & 1) ? 1.0f : -1.0f) * dist * tanHalfFovX;
            const float y    = ((i & 2) ? 1.0f : -1.0f) * dist * tanHalfFovY;
            corners[i] = glm::vec3(x, y, z);
        }
    } else {
        // For an orthographic view camera just use a unit box scaled by farZ
        for (int i = 0; i < 8; ++i) {
            const float z = (i < 4) ? -nearZ : -farZ;
            corners[i] = glm::vec3(
                (i & 1) ? 1.0f : -1.0f,
                (i & 2) ? 1.0f : -1.0f,
                z);
        }
    }

    // ── 3. Bounding sphere of frustum, computed in view space ────────────────
    // The sphere radius depends only on the frustum shape (fov, aspect, near,
    // far), not on the camera orientation, so it is rotationally invariant.
    // Computing it in view space (before transforming to world) gives optimal
    // floating-point stability.
    glm::vec3 sphereCenter(0.0f);
    for (auto const& c : corners)
        sphereCenter += c;
    sphereCenter /= 8.0f;

    float sphereRadius = 0.0f;
    for (auto const& c : corners)
        sphereRadius = std::max(sphereRadius, glm::length(c - sphereCenter));

    // ── 4. Transform sphere center to world space ────────────────────────────
    const glm::mat4 invView      = viewTransform.AsMatrix(); // view -> world
    const glm::vec3 sphereCenterWS = glm::vec3(invView * glm::vec4(sphereCenter, 1.0f));

    // ── 5. Build a temporary light-view matrix aligned to the light dir ──────
    const glm::vec3 lightDir = glm::normalize(light.m_direction);
    const glm::vec3 worldUp =
        (std::abs(lightDir.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                        : glm::vec3(0.0f, 0.0f, 1.0f);

    // lookAt from origin toward lightDir: produces a view matrix whose -Z = lightDir
    const glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, worldUp);

    // ── 6. Project sphere center into light space ────────────────────────────
    const glm::vec3 sphereCenterLS = glm::vec3(lightView * glm::vec4(sphereCenterWS, 1.0f));

    // ── 7. Texel snapping using the stable sphere-derived grid ───────────────
    // The ortho projection covers exactly 2*radius in XY, so the texel size is
    // (2*radius)/shadowMapSize.  Because the radius is rotationally invariant,
    // this grid size never changes as the camera rotates, eliminating swimming.
    const float texelSize = (2.0f * sphereRadius) / static_cast<float>(shadowMapSize);
    const float snappedX  = std::floor(sphereCenterLS.x / texelSize) * texelSize;
    const float snappedY  = std::floor(sphereCenterLS.y / texelSize) * texelSize;

    // ── 8. Position the light camera ─────────────────────────────────────────
    // Pull back along the light direction by radius * (1 + shadowBehind) so
    // that near=0 sits comfortably behind all potential shadow casters.
    // shadowBehind is a multiplier on sphereRadius: 0 = no extra, 1 = one
    // extra radius behind the sphere, etc.
    const glm::vec3 lightCamLS(snappedX, snappedY,
                                sphereCenterLS.z + sphereRadius * (1.0f + shadowBehind));

    const float orthoNear = 0.0f;
    const float orthoFar  = sphereRadius * (2.0f + shadowBehind);

    // Back-project the camera position to world space
    const glm::mat4 invLightView = glm::inverse(lightView);
    const glm::vec3 lightCamWS   = glm::vec3(invLightView * glm::vec4(lightCamLS, 1.0f));

    // ── 9. Build Transform (rotation from lookAt model matrix) ───────────────
    const glm::mat4 lightCamModel = glm::inverse(
        glm::lookAt(lightCamWS, lightCamWS + lightDir, worldUp));

    const glm::quat lightCamRot = glm::quat_cast(glm::mat3(lightCamModel));
    const Transform lightTransform(lightCamWS, lightCamRot, 1.0f);

    // ── 10. Orthographic camera sized to 2 * sphere radius ───────────────────
    const Camera shadowCamera = Camera::Orthographic(
        2.0f * sphereRadius,
        2.0f * sphereRadius,
        orthoNear,
        orthoFar);

    return ShadowCascade{ shadowCamera, lightTransform };
}
