#include "light.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <limits>

using namespace okami;

ShadowCascade okami::ComputeShadowCascade(
    DirectionalLightComponent const& light,
    Camera                    const& viewCamera,
    Transform                 const& viewTransform,
    glm::ivec2                const& viewportSize,
    float                            shadowDistance,
    int                              shadowMapSize,
    float                            shadowBehind)
{
    // ── 1. Extract projection parameters ────────────────────────────────────
    float nearZ = 0.1f, farZ = shadowDistance;
    float tanHalfFovY = 1.0f, aspect = 1.0f;
    bool  isPerspective = false;

    std::visit([&](auto const& proj) {
        using T = std::decay_t<decltype(proj)>;
        if constexpr (std::is_same_v<T, PerspectiveProjection>) {
            nearZ        = proj.m_nearZ;
            farZ         = std::min(proj.m_farZ, shadowDistance);
            tanHalfFovY  = std::tan(proj.m_fovY * 0.5f);
            aspect       = proj.m_aspectRatio.value_or(
                static_cast<float>(viewportSize.x) / static_cast<float>(viewportSize.y));
            isPerspective = true;
        } else if constexpr (std::is_same_v<T, OrthographicProjection>) {
            nearZ = proj.m_nearZ;
            farZ  = std::min(proj.m_farZ, shadowDistance);
        }
    }, viewCamera.m_projection);

    // ── 2. Build 8 frustum corners in view space, then transform to world ───
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

    const glm::mat4 invView = viewTransform.AsMatrix(); // world = invView * viewSpace
    for (auto& c : corners) {
        c = glm::vec3(invView * glm::vec4(c, 1.0f));
    }

    // ── 3. Build a temporary light-view matrix aligned to the light dir ─────
    const glm::vec3 lightDir = glm::normalize(light.m_direction);
    // Choose an up vector that isn't parallel to the light direction
    const glm::vec3 worldUp =
        (std::abs(lightDir.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                        : glm::vec3(0.0f, 0.0f, 1.0f);

    // lookAt from origin toward lightDir: produces a view matrix whose -Z = lightDir
    const glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, worldUp);

    // ── 4. Compute AABB of frustum corners in light view space ──────────────
    glm::vec3 mn( std::numeric_limits<float>::max());
    glm::vec3 mx(-std::numeric_limits<float>::max());
    for (auto const& c : corners) {
        const glm::vec3 lc = glm::vec3(lightView * glm::vec4(c, 1.0f));
        mn = glm::min(mn, lc);
        mx = glm::max(mx, lc);
    }

    // ── 5. Position the light camera ────────────────────────────────────────
    // In light-view space, -Z is the direction the light travels.
    // mn.z holds the most negative Z (furthest point the light will reach).
    // mx.z holds the nearest point to the notional light origin.
    //
    // We place the camera at the XY centre of the bounding box, and at
    // Z = mx.z + shadowBehind so that nearZ=0 sits behind all casters.
    const glm::vec3 lightCamLS(
        (mn.x + mx.x) * 0.5f,
        (mn.y + mx.y) * 0.5f,
        mx.z + shadowBehind);

    const float orthoNear = 0.0f;
    const float orthoFar  = lightCamLS.z - mn.z;  // full depth extent

    // ── 5a. Texel snapping ──────────────────────────────────────────────────
    // Round the light camera's light-space XY to the nearest shadow-texel
    // boundary so the projection only shifts in discrete texel-sized steps as
    // the view camera moves.  This eliminates sub-texel shadow swimming.
    const float texelW = (mx.x - mn.x) / static_cast<float>(shadowMapSize);
    const float texelH = (mx.y - mn.y) / static_cast<float>(shadowMapSize);
    const float snappedX = std::floor(lightCamLS.x / texelW) * texelW;
    const float snappedY = std::floor(lightCamLS.y / texelH) * texelH;

    // Back-project the camera position to world space
    const glm::mat4 invLightView = glm::inverse(lightView);
    const glm::vec3 lightCamWS   = glm::vec3(invLightView * glm::vec4(snappedX, snappedY, lightCamLS.z, 1.0f));

    // ── 6. Build Transform (rotation from lookAt model matrix) ──────────────
    // The camera model matrix is the inverse of the view matrix.
    const glm::mat4 lightCamModel = glm::inverse(
        glm::lookAt(lightCamWS, lightCamWS + lightDir, worldUp));

    const glm::quat lightCamRot = glm::quat_cast(glm::mat3(lightCamModel));
    const Transform lightTransform(lightCamWS, lightCamRot, 1.0f);

    // ── 7. Orthographic camera sized to the frustum AABB ────────────────────
    const Camera shadowCamera = Camera::Orthographic(
        mx.x - mn.x,
        mx.y - mn.y,
        orthoNear,
        orthoFar);

    return ShadowCascade{ shadowCamera, lightTransform };
}
