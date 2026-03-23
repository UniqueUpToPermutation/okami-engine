#pragma once

#include "renderer.hpp"   // StaticMeshComponent, Engine, texture/geometry/material headers
#include "transform.hpp"  // Transform
#include "animation.hpp"  // AnimationPlayerComponent, SkinnedMeshComponent, SkinData

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>

#include <filesystem>
#include <vector>
#include <memory>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace okami {

    // ─────────────────────────────────────────────────────────────────────────
    // Parameters
    // ─────────────────────────────────────────────────────────────────────────

    struct GltfSceneLoadParams {
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Prototype data types
    // ─────────────────────────────────────────────────────────────────────────

    // CPU-side description of one material slot from the GLTF asset.
    struct GltfSceneMaterialDef {
        // Absolute path to the base-colour (albedo) texture file.  sRGB.
        // Empty means "no texture found" – a 1×1 white fallback is used at spawn time.
        std::filesystem::path m_colorTexturePath;
        glm::vec4             m_colorTint{1.0f};

        // Absolute path to the tangent-space normal map texture file.  Linear.
        // Empty means no normal map is present for this material.
        std::filesystem::path m_normalTexturePath;
    };

    // One flattened renderable primitive: world transform + raw geometry + material index.
    // Geometry is move-only; this struct is therefore non-copyable.
    struct GltfSceneMeshInstance {
        Transform m_worldTransform;
        Geometry  m_geometry;
        int       m_materialIndex = -1;  // index into GltfSceneDesc::m_materials; -1 = none
    };

    // Skin data captured from one GLTF skin block.
    struct GltfSkinDef {
        std::string              m_name;
        std::shared_ptr<SkinData> m_skinData; // inverse bind matrices + joint names
    };

    // One skinned mesh primitive: like GltfSceneMeshInstance but with a skin reference.
    struct GltfSkinnedMeshInstance {
        Transform m_worldTransform;
        Geometry  m_geometry;
        int       m_materialIndex = -1;
        int       m_skinIndex     = -1; // index into GltfSceneDesc::m_skins
    };

    // The complete scene prototype.  Produced by GltfScene::FromFile() and
    // consumed (moved) by SpawnGltfScene().
    struct GltfSceneDesc {
        std::vector<GltfSceneMeshInstance>    m_meshInstances;
        std::vector<GltfSkinnedMeshInstance>  m_skinnedMeshInstances;
        std::vector<GltfSceneMaterialDef>     m_materials;
        std::vector<GltfSkinDef>              m_skins;

        // Ozz runtime assets loaded from .skeleton.ozz / .animation.ozz alongside the GLTF.
        // May be null/empty when the GLTF has no animations or the archives were not found.
        std::shared_ptr<ozz::animation::Skeleton>                m_skeleton;
        std::vector<std::shared_ptr<ozz::animation::Animation>>  m_animations;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // GltfScene resource type (satisfies the ResourceType concept)
    // ─────────────────────────────────────────────────────────────────────────

    class GltfScene {
    public:
        using Desc       = GltfSceneDesc;
        using LoadParams = GltfSceneLoadParams;

        GltfSceneDesc m_data;

        OKAMI_NO_COPY(GltfScene);
        OKAMI_MOVE(GltfScene);

        GltfScene() = default;
        explicit GltfScene(GltfSceneDesc data) : m_data(std::move(data)) {}

        // Synchronously load and parse a GLTF/GLB file, building the scene
        // prototype.  The returned GltfScene owns all the parsed Geometry data.
        // On failure an error is returned and nothing is loaded.
        static Expected<GltfScene> FromFile(
            std::filesystem::path const& path,
            GltfSceneLoadParams   const& params = {});
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Spawn helper
    // ─────────────────────────────────────────────────────────────────────────

    // Instantiate all mesh primitives described by the prototype as entities in
    // the engine world.  Each entity receives a StaticMeshComponent (with a
    // freshly uploaded GeometryHandle and a MaterialHandle) and a Transform.
    //
    // The prototype is consumed (moved): geometry data is transferred to the GPU
    // and can no longer be accessed through the desc after this call.
    //
    // Materials that reference a missing texture fall back to a programmatically
    // generated 1×1 white texture.
    //
    // An optional root transform can be supplied to reposition the entire scene.
    void SpawnGltfScene(
        Engine&          en,
        GltfSceneDesc&&  proto,
        Transform const& root  = Transform::Identity(),
        std::string_view name  = "GltfScene");

} // namespace okami
