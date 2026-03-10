#pragma once

// tinygltf – include declarations only (implementation lives in gltf.cpp)
#include <tiny_gltf.h>

#include "../sample.hpp"
#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "material.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "sky.hpp"
#include "camera.hpp"
#include "camera_controllers.hpp"
#include "ogl/ogl_renderer.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>
#include <glog/logging.h>

namespace sample_sponza {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Maps a GLTF attribute semantic to the engine's AttributeType.
static okami::AttributeType MapGLTFAttrib(std::string const& name) {
    if (name == "POSITION")   return okami::AttributeType::Position;
    if (name == "NORMAL")     return okami::AttributeType::Normal;
    if (name == "TEXCOORD_0") return okami::AttributeType::TexCoord;
    if (name == "COLOR_0")    return okami::AttributeType::Color;
    if (name == "TANGENT")    return okami::AttributeType::Tangent;
    return okami::AttributeType::Unknown;
}

// Maps a GLTF component type to the engine's AccessorComponentType.
static okami::AccessorComponentType MapGLTFComponent(int c) {
    switch (c) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return okami::AccessorComponentType::UByte;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return okami::AccessorComponentType::UShort;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return okami::AccessorComponentType::UInt;
        default:                                     return okami::AccessorComponentType::UInt;
    }
}

// Builds a compact, single-buffer Geometry from one GLTF mesh primitive.
// Each attribute's data is extracted element-by-element from the source
// buffer, de-interleaved, and packed tightly.  The resulting Geometry
// owns its own buffer and has stride = 0 (tightly packed).
static okami::Geometry BuildPrimitiveGeometry(
    tinygltf::Model   const& model,
    tinygltf::Primitive const& prim)
{
    std::vector<uint8_t>      packed;
    okami::GeometryPrimitiveDesc primDesc;
    primDesc.m_type = okami::MeshType::Static;

    // ── Indices ────────────────────────────────────────────────────────────
    if (prim.indices >= 0) {
        auto const& acc = model.accessors[prim.indices];
        auto const& bv  = model.bufferViews[acc.bufferView];
        auto const& buf = model.buffers[bv.buffer];

        uint32_t compSize  = okami::GetSize(okami::AccessorType::Scalar,
                                            MapGLTFComponent(acc.componentType));
        uint32_t srcStride = (bv.byteStride != 0)
                            ? static_cast<uint32_t>(bv.byteStride)
                            : compSize;
        size_t   srcBase   = bv.byteOffset + acc.byteOffset;

        okami::IndexInfo idx;
        idx.m_type   = MapGLTFComponent(acc.componentType);
        idx.m_buffer = 0;
        idx.m_count  = acc.count;
        idx.m_offset = packed.size();

        for (size_t i = 0; i < acc.count; ++i) {
            auto it = buf.data.begin() + srcBase + i * srcStride;
            packed.insert(packed.end(), it, it + compSize);
        }
        primDesc.m_indices = idx;
    }

    // ── Vertex attributes ─────────────────────────────────────────────────
    size_t vertexCount = 0;
    for (auto const& [attribName, accessorIndex] : prim.attributes) {
        auto attrType = MapGLTFAttrib(attribName);
        if (attrType == okami::AttributeType::Unknown) continue;

        auto const& acc = model.accessors[accessorIndex];
        auto const& bv  = model.bufferViews[acc.bufferView];
        auto const& buf = model.buffers[bv.buffer];

        uint32_t elemSize  = okami::GetSize(attrType);
        uint32_t srcStride = (bv.byteStride != 0)
                            ? static_cast<uint32_t>(bv.byteStride)
                            : elemSize;
        size_t   srcBase   = bv.byteOffset + acc.byteOffset;

        okami::Attribute attr;
        attr.m_type   = attrType;
        attr.m_buffer = 0;
        attr.m_offset = packed.size();
        attr.m_stride = 0; // tightly packed after extraction
        primDesc.m_attributes.emplace(attrType, attr);

        for (size_t i = 0; i < acc.count; ++i) {
            auto it = buf.data.begin() + srcBase + i * srcStride;
            packed.insert(packed.end(), it, it + elemSize);
        }

        if (vertexCount == 0) vertexCount = acc.count;
    }
    primDesc.m_vertexCount = vertexCount;

    okami::GeometryDesc desc;
    desc.m_primitives.push_back(std::move(primDesc));

    std::vector<std::vector<uint8_t>> buffers;
    buffers.push_back(std::move(packed));

    return okami::Geometry(std::move(buffers), std::move(desc));
}

// Converts a GLTF node's TRS components (or a 4x4 matrix) to a Transform.
static okami::Transform NodeTransform(tinygltf::Node const& node) {
    if (!node.matrix.empty()) {
        glm::mat4 m = glm::make_mat4(node.matrix.data());

        glm::vec3 pos(m[3]);
        float sx = glm::length(glm::vec3(m[0]));
        float sy = glm::length(glm::vec3(m[1]));
        float sz = glm::length(glm::vec3(m[2]));

        glm::mat3 rotMat(
            glm::vec3(m[0]) / sx,
            glm::vec3(m[1]) / sy,
            glm::vec3(m[2]) / sz
        );
        glm::quat rot = glm::quat_cast(rotMat);
        glm::mat3 scale(sx, 0, 0,  0, sy, 0,  0, 0, sz);

        return okami::Transform(pos, rot, scale);
    }

    glm::vec3 pos(0.0f);
    if (!node.translation.empty())
        pos = { node.translation[0], node.translation[1], node.translation[2] };

    glm::quat rot = glm::identity<glm::quat>();
    if (!node.rotation.empty()) {
        // GLTF stores quaternion as [x, y, z, w]; GLM ctor is (w, x, y, z).
        rot = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])
        );
    }

    glm::mat3 scaleM = glm::identity<glm::mat3>();
    if (!node.scale.empty()) {
        float sx = static_cast<float>(node.scale[0]);
        float sy = static_cast<float>(node.scale[1]);
        float sz = static_cast<float>(node.scale[2]);
        scaleM = glm::mat3(sx, 0, 0,  0, sy, 0,  0, 0, sz);
    }

    return okami::Transform(pos, rot, scaleM);
}

// Recursively walks the scene node hierarchy and spawns one entity per
// mesh primitive, inheriting the accumulated parent world transform.
static void SpawnNodes(
    okami::Engine& en,
    tinygltf::Model                      const& model,
    std::vector<int>                     const& nodeIndices,
    std::vector<okami::MaterialHandle>   const& materials,
    okami::Transform                     const& parentWorld)
{
    for (int ni : nodeIndices) {
        auto const& node   = model.nodes[ni];
        auto         world = parentWorld * NodeTransform(node);

        if (node.mesh >= 0) {
            auto const& mesh = model.meshes[node.mesh];
            for (auto const& prim : mesh.primitives) {
                auto geoH = en.CreateGeometry(BuildPrimitiveGeometry(model, prim));

                okami::MaterialHandle matH;
                if (prim.material >= 0 &&
                    prim.material < static_cast<int>(materials.size()))
                    matH = materials[prim.material];

                auto entity = en.CreateEntity();
                en.AddComponent(entity, okami::StaticMeshComponent{ geoH, matH });
                en.AddComponent(entity, world);
            }
        }

        SpawnNodes(en, model, node.children, materials, world);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sample
// ─────────────────────────────────────────────────────────────────────────────

class SponzaSample : public okami::Sample {
public:
    // When true, the extension of every texture URI from the GLTF is replaced
    // with .ktx2, loading the pre-converted KTX2 version produced by png2ktx.
    bool m_useKtx2 = true;

    void SetupModules(
        okami::Engine& en,
        std::optional<okami::HeadlessGLParams> headless = {}) override
    {
        if (headless) {
            en.CreateModule<okami::GLFWModuleFactory>({}, std::move(*headless));
        } else {
            en.CreateModule<okami::GLFWModuleFactory>();
        }
        en.CreateModule<okami::OGLRendererFactory>({}, okami::RendererParams{});
        en.CreateModule<okami::CameraControllerModuleFactory>();
    }

    void SetupScene(okami::Engine& en) override {
        using namespace okami;

        // ── Load GLTF file ─────────────────────────────────────────────────
        auto gltfPath = GetSampleAssetPath("sponza/Sponza.gltf");
        auto basePath = gltfPath.parent_path();

        // Tell tinygltf not to decode images into memory; we'll load them
        // through the engine's texture manager instead.
        static constexpr auto kNoopImageLoader = [](
            tinygltf::Image*, const int, std::string*, std::string*,
            int, int, const unsigned char*, int, void*) -> bool {
            return true; // mark loaded (but skip actual decode)
        };

        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(+kNoopImageLoader, nullptr);

        tinygltf::Model model;
        std::string err, warn;
        bool ok = loader.LoadASCIIFromFile(&model, &err, &warn, gltfPath.string());
        if (!warn.empty()) LOG(WARNING) << "[Sponza] GLTF warning: " << warn;
        if (!ok || !err.empty()) {
            LOG(ERROR) << "[Sponza] Failed to load " << gltfPath << ": " << err;
            return;
        }

        // ── Textures ───────────────────────────────────────────────────────
        // Helper: optionally rewrite the extension to .ktx2.
        auto resolveImgPath = [&](std::filesystem::path p) -> std::filesystem::path {
            if (m_useKtx2)
                p.replace_extension(".ktx2");
            return p;
        };

        // Pre-load a white fallback for materials with no base colour texture.
        TextureHandle whiteHandle = en.LoadTexture(resolveImgPath(basePath / "white.png"));

        // One handle per GLTF image.
        std::vector<TextureHandle> imageHandles;
        imageHandles.reserve(model.images.size());
        for (auto const& image : model.images) {
            if (!image.uri.empty()) {
                auto imgPath = resolveImgPath(basePath / image.uri);
                if (std::filesystem::exists(imgPath)) {
                    imageHandles.push_back(en.LoadTexture(imgPath));
                    continue;
                }
                LOG(WARNING) << "[Sponza] Texture not found: " << imgPath;
            }
            imageHandles.push_back(whiteHandle);
        }

        // Resolve a GLTF texture index to a TextureHandle.
        auto resolveTexture = [&](int texIndex) -> TextureHandle {
            if (texIndex < 0 ||
                texIndex >= static_cast<int>(model.textures.size()))
                return whiteHandle;
            int imgIdx = model.textures[texIndex].source;
            if (imgIdx < 0 ||
                imgIdx >= static_cast<int>(imageHandles.size()))
                return whiteHandle;
            return imageHandles[imgIdx];
        };

        // ── Materials ──────────────────────────────────────────────────────
        std::vector<MaterialHandle> materials;
        materials.reserve(model.materials.size());
        for (auto const& mat : model.materials) {
            BasicTexturedMaterial bm;
            int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
            bm.m_colorTexture = resolveTexture(texIdx);
            if (!bm.m_colorTexture)
                bm.m_colorTexture = whiteHandle;

            auto const& cf = mat.pbrMetallicRoughness.baseColorFactor;
            if (cf.size() == 4)
                bm.m_colorTint = glm::vec4(cf[0], cf[1], cf[2], cf[3]);

            materials.push_back(en.CreateMaterial(std::move(bm)));
        }

        // ── Scene graph → entities ─────────────────────────────────────────
        int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
        if (sceneIdx < static_cast<int>(model.scenes.size())) {
            SpawnNodes(en, model, model.scenes[sceneIdx].nodes,
                       materials, Transform::Identity());
        }

        // ── Sky ────────────────────────────────────────────────────────────
        {
            auto skyEntity = en.CreateEntity();
            en.AddComponent(skyEntity, SkyComponent{});
        }

        // ── First-person camera ────────────────────────────────────────────
        // Initial orientation: facing -Z (yaw=0, pitch=0).
        // Adjust startPos / initRot to taste once you know the model's layout.
        glm::quat initRot =
            glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        auto cameraEntity = en.CreateEntity();
        en.AddComponent(cameraEntity,
            Camera::Perspective(glm::half_pi<float>(), 0.1f, 1000.0f));
        en.AddComponent(cameraEntity,
            Transform(glm::vec3(0.0f, 1.8f, 0.0f), initRot, 1.0f));
        en.AddComponent(cameraEntity, FirstPersonCameraControllerComponent{
            .m_moveSpeed       = 10.0f,
            .m_lookSensitivity = 0.002f,
        });
        en.SetActiveCamera(cameraEntity);
    }
};

} // namespace sample_sponza
