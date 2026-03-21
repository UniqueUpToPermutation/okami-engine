#pragma once

#include "common.hpp"
#include "geometry.hpp"
#include "material.hpp"
#include "module.hpp"
#include "entity_manager.hpp"

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <memory>
#include <string>
#include <vector>

namespace okami {

    // ---------------------------------------------------------------------------
    // SkinData
    //
    // Inverse bind matrices for one GLTF skin, plus a per-joint name list used
    // to resolve skin joint indices into ozz skeleton joint indices at spawn time.
    // ---------------------------------------------------------------------------
    struct SkinData {
        // One matrix per skin joint (in the same order as GLTF skin.joints[]).
        std::vector<glm::mat4> m_inverseBindMatrices;

        // Joint names, one per skin joint slot (matches m_inverseBindMatrices).
        std::vector<std::string> m_jointNames;

        // Resolved mapping: skin joint index -> ozz skeleton joint index.
        // Populated by SpawnGltfScene() after the skeleton is known.
        std::vector<int> m_skeletonJointIndices;
    };

    // ---------------------------------------------------------------------------
    // SkeletonComponent
    //
    // Attach to a "skeleton entity". Owns the compiled skeleton and animation
    // clips, plus user-facing playback parameters. This component is read-only
    // during the job graph; the animation module sends UpdateComponentSignal to
    // update playback parameters through the normal frame-merge path.
    // ---------------------------------------------------------------------------
    struct SkeletonComponent {
        std::shared_ptr<ozz::animation::Skeleton>                m_skeleton;
        std::vector<std::shared_ptr<ozz::animation::Animation>>  m_animations;

        // Playback parameters (written by user code between frames).
        int   m_currentAnimation = 0;
        bool  m_loop             = true;
        float m_speed            = 1.0f;
    };

    // ---------------------------------------------------------------------------
    // SkeletonStateComponent
    //
    // Holds the per-frame sampled transform output for one skeleton entity.
    // Updated each frame via UpdateComponentSignal<SkeletonStateComponent> sent
    // from the animation system job. Read by OGLSkinnedMeshRenderer to compute
    // skinning matrices.
    //
    // NOTE: The SamplingJob::Context (coherency cache) is NOT stored here because
    // it requires write access during sampling — it lives in the animation
    // module's per-entity context map instead.
    // ---------------------------------------------------------------------------
    struct SkeletonStateComponent {
        // Local-space SoA transforms — output of SamplingJob.
        std::vector<ozz::math::SoaTransform>  m_localTransforms;
        // Model-space matrices — output of LocalToModelJob.
        std::vector<ozz::math::Float4x4>      m_modelMatrices;
        // Current playback time in seconds.
        float m_time = 0.0f;

        bool IsReady() const { return !m_modelMatrices.empty(); }
    };

    // ---------------------------------------------------------------------------
    // SkinnedMeshComponent
    //
    // Attach to each renderable skinned mesh primitive. References the geometry
    // (uploaded with MeshType::Skinned layout), material, skin data (inverse bind
    // matrices + joint name mapping), and the skeleton entity that drives it.
    // ---------------------------------------------------------------------------
    struct SkinnedMeshComponent {
        GeometryHandle            m_geometry;
        MaterialHandle            m_material;
        std::shared_ptr<SkinData> m_skinData;
        // The entity that carries SkeletonComponent + SkeletonStateComponent.
        entity_t                  m_skeletonEntity = kNullEntity;
    };

    // ---------------------------------------------------------------------------
    // AnimationSystemModuleFactory
    //
    // Creates an EngineModule that updates all AnimationPlayerComponent entities
    // each frame (advances time, samples animation, resolves model matrices).
    // ---------------------------------------------------------------------------
    struct AnimationSystemModuleFactory {
        std::unique_ptr<EngineModule> operator()() const;
    };

} // namespace okami
