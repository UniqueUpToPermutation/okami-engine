#pragma once

#include "material.hpp"

#include <glm/vec3.hpp>

namespace okami {
    struct SkyComponent {
        MaterialHandle m_skyMaterial;
    };

    struct SkyDefaultMaterial {
    };

    // Preetham / Hosek-Wilkie style atmospheric scattering sky.
    // All defaults produce a clear blue daytime sky with the sun near zenith.
    struct SkyAtmosphereMaterial {
        float     depolarizationFactor       = 0.035f;
        float     mieCoefficient             = 0.005f;
        float     mieDirectionalG            = 0.8f;
        glm::vec3 mieKCoefficient            = {0.686f, 0.678f, 0.666f};
        float     mieV                       = 4.0f;
        float     mieZenithLength            = 434000.0f;
        float     numMolecules               = 2.542e25f;
        glm::vec3 primaries                  = {6.8e-7f, 5.5e-7f, 4.5e-7f};
        float     rayleigh                   = 1.0f;
        float     rayleighZenithLength       = 8400.0f;
        float     refractiveIndex            = 1.0003f;
        // Cosine of the sun's angular diameter (~0.5 deg); fed into cos() in the shader.
        float     sunAngularDiameterDegrees  = 0.00872665f;
        float     sunIntensityFactor         = 1000.0f;
        float     sunIntensityFalloffSteepness = 1.5f;
        // World-space position used only for direction – distance is irrelevant.
        glm::vec3 sunPosition                = {100000.0f, 50000.0f, 0.0f};
        float     turbidity                  = 2.0f;
    };
}