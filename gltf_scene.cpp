#include "gltf_scene.hpp"

#include "material.hpp"   // LambertMaterial, DefaultMaterial

#include <tiny_gltf.h>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>
#include <glog/logging.h>

#include <filesystem>
#include <sstream>

namespace okami {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static AttributeType MapGLTFAttrib(std::string const& name) {
    if (name == "POSITION")   return AttributeType::Position;
    if (name == "NORMAL")     return AttributeType::Normal;
    if (name == "TEXCOORD_0") return AttributeType::TexCoord;
    if (name == "COLOR_0")    return AttributeType::Color;
    if (name == "TANGENT")    return AttributeType::Tangent;
    if (name == "JOINTS_0")   return AttributeType::Joints;
    if (name == "WEIGHTS_0")  return AttributeType::Weights;
    return AttributeType::Unknown;
}

static AccessorComponentType MapGLTFComponent(int c) {
    switch (c) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return AccessorComponentType::UByte;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return AccessorComponentType::UShort;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return AccessorComponentType::UInt;
        default:                                      return AccessorComponentType::UInt;
    }
}

// Builds a compact, single-buffer Geometry from one GLTF mesh primitive.
// isSkinned: set true when the owning node references a skin.
static Geometry BuildPrimitiveGeometry(
    tinygltf::Model    const& model,
    tinygltf::Primitive const& prim,
    bool                       isSkinned = false)
{
    std::vector<uint8_t>  packed;
    GeometryPrimitiveDesc primDesc;
    primDesc.m_type = isSkinned ? MeshType::Skinned : MeshType::Static;

    // Indices
    if (prim.indices >= 0) {
        auto const& acc = model.accessors[prim.indices];
        auto const& bv  = model.bufferViews[acc.bufferView];
        auto const& buf = model.buffers[bv.buffer];

        uint32_t compSize  = GetSize(AccessorType::Scalar, MapGLTFComponent(acc.componentType));
        uint32_t srcStride = (bv.byteStride != 0)
                           ? static_cast<uint32_t>(bv.byteStride)
                           : compSize;
        size_t   srcBase   = bv.byteOffset + acc.byteOffset;

        IndexInfo idx;
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

    // Vertex attributes
    size_t vertexCount = 0;
    for (auto const& [attribName, accessorIndex] : prim.attributes) {
        auto attrType = MapGLTFAttrib(attribName);
        if (attrType == AttributeType::Unknown) continue;

        auto const& acc = model.accessors[accessorIndex];
        auto const& bv  = model.bufferViews[acc.bufferView];
        auto const& buf = model.buffers[bv.buffer];

        // JOINTS_0 may be stored as UNSIGNED_BYTE or UNSIGNED_SHORT in the GLTF buffer,
        // but our geometry pipeline stores Joints as vec4 (4 floats).  Convert here.
        if (attrType == AttributeType::Joints) {
            const size_t dstElemSize = sizeof(glm::vec4); // 16 bytes (4 floats)
            Attribute attr;
            attr.m_type   = attrType;
            attr.m_buffer = 0;
            attr.m_offset = packed.size();
            attr.m_stride = 0;
            primDesc.m_attributes.emplace(attrType, attr);

            const int srcCompSize = tinygltf::GetComponentSizeInBytes(acc.componentType);
            const int numComp     = 4; // VEC4

            size_t srcBase = bv.byteOffset + acc.byteOffset;
            uint32_t srcStride = (bv.byteStride != 0)
                               ? static_cast<uint32_t>(bv.byteStride)
                               : static_cast<uint32_t>(srcCompSize * numComp);

            for (size_t i = 0; i < acc.count; ++i) {
                const uint8_t* src = buf.data.data() + srcBase + i * srcStride;
                glm::vec4 joints4(0.0f);
                for (int c = 0; c < numComp; ++c) {
                    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        joints4[c] = static_cast<float>(src[c]);
                    } else { // UNSIGNED_SHORT
                        uint16_t v;
                        std::memcpy(&v, src + c * 2, 2);
                        joints4[c] = static_cast<float>(v);
                    }
                }
                auto* dst = reinterpret_cast<uint8_t const*>(&joints4);
                packed.insert(packed.end(), dst, dst + dstElemSize);
            }
            if (vertexCount == 0) vertexCount = acc.count;
            continue;
        }

        uint32_t elemSize  = GetSize(attrType);
        uint32_t srcStride = (bv.byteStride != 0)
                           ? static_cast<uint32_t>(bv.byteStride)
                           : elemSize;
        size_t   srcBase   = bv.byteOffset + acc.byteOffset;

        Attribute attr;
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

    GeometryDesc desc;
    desc.m_primitives.push_back(std::move(primDesc));

    std::vector<std::vector<uint8_t>> buffers;
    buffers.push_back(std::move(packed));

    return Geometry(std::move(buffers), std::move(desc));
}

// Converts a GLTF node's TRS components (or 4×4 matrix) to an okami Transform.
static Transform NodeTransform(tinygltf::Node const& node) {
    if (!node.matrix.empty()) {
        glm::mat4 m = glm::make_mat4(node.matrix.data());

        glm::vec3 pos(m[3]);
        float sx = glm::length(glm::vec3(m[0]));
        float sy = glm::length(glm::vec3(m[1]));
        float sz = glm::length(glm::vec3(m[2]));

        glm::mat3 rotMat(
            glm::vec3(m[0]) / sx,
            glm::vec3(m[1]) / sy,
            glm::vec3(m[2]) / sz);
        glm::quat rot      = glm::quat_cast(rotMat);
        glm::mat3 scaleMat = glm::mat3(sx, 0, 0, 0, sy, 0, 0, 0, sz);

        return Transform(pos, rot, scaleMat);
    }

    glm::vec3 pos(0.0f);
    if (!node.translation.empty())
        pos = {node.translation[0], node.translation[1], node.translation[2]};

    glm::quat rot = glm::identity<glm::quat>();
    if (!node.rotation.empty()) {
        // GLTF stores [x, y, z, w]; GLM ctor is (w, x, y, z).
        rot = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
    }

    glm::mat3 scaleM = glm::identity<glm::mat3>();
    if (!node.scale.empty()) {
        float sx = static_cast<float>(node.scale[0]);
        float sy = static_cast<float>(node.scale[1]);
        float sz = static_cast<float>(node.scale[2]);
        scaleM   = glm::mat3(sx, 0, 0, 0, sy, 0, 0, 0, sz);
    }

    return Transform(pos, rot, scaleM);
}

// Recursively walks the GLTF node tree and appends mesh instances per primitive.
// Skinned nodes (node.skin >= 0) go into out.m_skinnedMeshInstances; others into
// out.m_meshInstances.
static void BuildNodes(
    tinygltf::Model        const& model,
    std::vector<int>       const& nodeIndices,
    Transform              const& parentWorld,
    GltfSceneDesc&                out)
{
    for (int ni : nodeIndices) {
        auto const& node  = model.nodes[ni];
        auto         world = parentWorld * NodeTransform(node);

        if (node.mesh >= 0) {
            bool isSkinned = (node.skin >= 0);
            for (auto const& prim : model.meshes[node.mesh].primitives) {
                if (isSkinned) {
                    GltfSkinnedMeshInstance inst;
                    inst.m_worldTransform = world;
                    inst.m_geometry       = BuildPrimitiveGeometry(model, prim, /*isSkinned=*/true);
                    inst.m_materialIndex  = prim.material;
                    inst.m_skinIndex      = node.skin;
                    out.m_skinnedMeshInstances.push_back(std::move(inst));
                } else {
                    GltfSceneMeshInstance inst;
                    inst.m_worldTransform = world;
                    inst.m_geometry       = BuildPrimitiveGeometry(model, prim, /*isSkinned=*/false);
                    inst.m_materialIndex  = prim.material;
                    out.m_meshInstances.push_back(std::move(inst));
                }
            }
        }

        BuildNodes(model, node.children, world, out);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GltfScene::FromFile
// ─────────────────────────────────────────────────────────────────────────────

Expected<GltfScene> GltfScene::FromFile(
    std::filesystem::path const& path,
    GltfSceneLoadParams   const& params)
{
    // Suppress tinygltf's built-in image decoder; textures are loaded separately.
    static constexpr auto kNoopImageLoader = [](
        tinygltf::Image*, const int, std::string*, std::string*,
        int, int, const unsigned char*, int, void*) -> bool {
        return true;
    };

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(+kNoopImageLoader, nullptr);

    tinygltf::Model model;
    std::string     err, warn;

    auto ext = path.extension().string();
    bool ok  = false;
    if (ext == ".glb" || ext == ".GLB")
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
    else
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());

    if (!warn.empty()) {
        // TinyGLTF emits "File not found" / "Failed to load external 'uri' for
        // image" lines for every external image it can't resolve.  Since we use
        // a noop image loader and load textures via our own KTX2 pipeline, these
        // are expected noise – strip them before deciding whether to log.
        std::istringstream warnStream(warn);
        std::string warnLine, filteredWarn;
        while (std::getline(warnStream, warnLine)) {
            if (warnLine.find("File not found") != std::string::npos ||
                warnLine.find("Failed to load external 'uri' for image") != std::string::npos)
                continue;
            if (!filteredWarn.empty()) filteredWarn += '\n';
            filteredWarn += warnLine;
        }
        if (!filteredWarn.empty())
            LOG(WARNING) << "[GltfScene] " << path << ": " << filteredWarn;
    }
    if (!ok || !err.empty()) {
        LOG(ERROR) << "[GltfScene] Failed to load " << path << " – " << err;
        return OKAMI_UNEXPECTED("Failed to load GLTF file: " + err);
    }

    auto basePath = path.parent_path();

    GltfSceneDesc desc;

    // ── Materials ─────────────────────────────────────────────────────────
    desc.m_materials.reserve(model.materials.size());
    for (auto const& mat : model.materials) {
        GltfSceneMaterialDef def;

        // Base colour (albedo) – sRGB
        int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIdx >= 0 && texIdx < static_cast<int>(model.textures.size())) {
            int imgIdx = model.textures[texIdx].source;
            if (imgIdx >= 0 && imgIdx < static_cast<int>(model.images.size())) {
                auto const& img = model.images[imgIdx];
                if (!img.uri.empty()) {
                    std::filesystem::path texPath = basePath / img.uri;
                    def.m_colorTexturePath = std::move(texPath);
                }
            }
        }

        auto const& cf = mat.pbrMetallicRoughness.baseColorFactor;
        if (cf.size() == 4)
            def.m_colorTint = glm::vec4(cf[0], cf[1], cf[2], cf[3]);

        // Normal map – linear (tangent-space, not sRGB)
        int normalTexIdx = mat.normalTexture.index;
        if (normalTexIdx >= 0 && normalTexIdx < static_cast<int>(model.textures.size())) {
            int imgIdx = model.textures[normalTexIdx].source;
            if (imgIdx >= 0 && imgIdx < static_cast<int>(model.images.size())) {
                auto const& img = model.images[imgIdx];
                if (!img.uri.empty()) {
                    std::filesystem::path texPath = basePath / img.uri;
                    def.m_normalTexturePath = std::move(texPath);
                }
            }
        }

        desc.m_materials.push_back(std::move(def));
    }

    // ── Scene graph ────────────────────────────────────────────────────────
    int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIdx < static_cast<int>(model.scenes.size()))
        BuildNodes(model, model.scenes[sceneIdx].nodes, Transform::Identity(), desc);

    // ── Skins ──────────────────────────────────────────────────────────────
    for (auto const& skin : model.skins) {
        GltfSkinDef skinDef;
        skinDef.m_name     = skin.name;
        skinDef.m_skinData = std::make_shared<SkinData>();

        // Joint names.
        skinDef.m_skinData->m_jointNames.reserve(skin.joints.size());
        for (int ji : skin.joints) {
            if (ji >= 0 && ji < static_cast<int>(model.nodes.size()))
                skinDef.m_skinData->m_jointNames.push_back(model.nodes[ji].name);
            else
                skinDef.m_skinData->m_jointNames.push_back("");
        }

        // Inverse bind matrices.
        skinDef.m_skinData->m_inverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
        if (skin.inverseBindMatrices >= 0) {
            auto const& acc = model.accessors[skin.inverseBindMatrices];
            auto const& bv  = model.bufferViews[acc.bufferView];
            auto const& buf = model.buffers[bv.buffer];
            const uint8_t* src = buf.data.data() + bv.byteOffset + acc.byteOffset;
            size_t count = static_cast<size_t>(acc.count);
            if (count > skin.joints.size()) count = skin.joints.size();
            for (size_t i = 0; i < count; ++i) {
                std::memcpy(&skinDef.m_skinData->m_inverseBindMatrices[i],
                            src + i * sizeof(glm::mat4),
                            sizeof(glm::mat4));
            }
        }

        desc.m_skins.push_back(std::move(skinDef));
    }

    // ── Ozz archives ───────────────────────────────────────────────────────
    // Asset processor emits: {stem}.skeleton.ozz and {stem}.{name}.animation.ozz
    // alongside the GLTF file (in the same processed output directory).
    // We attempt to load them here; missing files are silently skipped (the GLTF
    // may not have animations, or the asset pipeline may not have run yet).
    {
        auto stem = (basePath / path.stem()).string();

        // Skeleton
        auto skelPath = std::filesystem::path(stem + ".skeleton.ozz");
        if (std::filesystem::exists(skelPath)) {
            ozz::io::File file(skelPath.string().c_str(), "rb");
            if (file.opened()) {
                ozz::io::IArchive archive(&file);
                if (archive.TestTag<ozz::animation::Skeleton>()) {
                    auto skel = std::make_shared<ozz::animation::Skeleton>();
                    archive >> *skel;
                    desc.m_skeleton = std::move(skel);
                    LOG(INFO) << "[GltfScene] Loaded skeleton: " << skelPath;
                }
            }
        }

        // Animations – search for all *.animation.ozz files with matching stem prefix.
        if (desc.m_skeleton) {
            for (auto const& anim : model.animations) {
                // Sanitize the name the same way the asset processor does.
                std::string safeName = anim.name;
                for (auto& c : safeName)
                    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
                        c = '_';
                auto animPath = std::filesystem::path(stem + "." + safeName + ".animation.ozz");
                if (!std::filesystem::exists(animPath)) {
                    LOG(WARNING) << "[GltfScene] Animation archive not found: " << animPath;
                    continue;
                }
                ozz::io::File file(animPath.string().c_str(), "rb");
                if (!file.opened()) continue;

                ozz::io::IArchive archive(&file);
                if (archive.TestTag<ozz::animation::Animation>()) {
                    auto clip = std::make_shared<ozz::animation::Animation>();
                    archive >> *clip;
                    LOG(INFO) << "[GltfScene] Loaded animation: " << animPath
                              << " (" << clip->name() << ", " << clip->duration() << "s)";
                    desc.m_animations.push_back(std::move(clip));
                }
            }
        }
    }

    return GltfScene{std::move(desc)};
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnGltfScene
// ─────────────────────────────────────────────────────────────────────────────

void SpawnGltfScene(Engine& en, GltfSceneDesc&& proto, Transform const& root) {
    // Build one material handle per GLTF material.
    // Materials with a resolvable texture path use LambertMaterial;
    // those without fall back to DefaultMaterial.
    std::vector<MaterialHandle> materials;
    materials.reserve(proto.m_materials.size());
    for (auto const& matDef : proto.m_materials) {
        // Pre-load the normal map (linear) first so it is cached with the
        // correct sRGB=false flag before anything else touches it.
        if (!matDef.m_normalTexturePath.empty() &&
            std::filesystem::exists(matDef.m_normalTexturePath)) {
            en.LoadTexture(matDef.m_normalTexturePath, TextureLoadParams{ .m_srgb = false });
        }

        if (!matDef.m_colorTexturePath.empty() &&
            std::filesystem::exists(matDef.m_colorTexturePath)) {
            LambertMaterial bm;
            // Albedo textures are sRGB – pass the flag so mips are built correctly.
            bm.m_colorTexture = en.LoadTexture(matDef.m_colorTexturePath,
                                               TextureLoadParams{ .m_srgb = true });
            // Normal map is linear.
            if (!matDef.m_normalTexturePath.empty() &&
                std::filesystem::exists(matDef.m_normalTexturePath)) {
                bm.m_normalTexture = en.LoadTexture(matDef.m_normalTexturePath,
                                                    TextureLoadParams{ .m_srgb = false });
            }
            bm.m_colorTint    = matDef.m_colorTint;
            materials.push_back(en.CreateMaterial(std::move(bm)));
        } else {
            materials.push_back(en.CreateMaterial(DefaultMaterial{}));
        }
    }

    // Spawn one entity per static mesh instance.
    for (auto& inst : proto.m_meshInstances) {
        auto geoH = en.CreateGeometry(std::move(inst.m_geometry));

        MaterialHandle matH;
        if (inst.m_materialIndex >= 0 &&
            inst.m_materialIndex < static_cast<int>(materials.size()))
            matH = materials[inst.m_materialIndex];

        auto world  = root * inst.m_worldTransform;
        auto entity = en.CreateEntity();
        en.AddComponent(entity, StaticMeshComponent{geoH, matH});
        en.AddComponent(entity, world);
    }

    // ── Skeleton entity ─────────────────────────────────────────────────────
    // One entity carries SkeletonComponent (read-only data + params) and
    // SkeletonStateComponent (per-frame sampled transforms, updated via
    // UpdateComponentSignal by the animation system each frame).
    entity_t skeletonEntity = kNullEntity;
    if (proto.m_skeleton && !proto.m_skinnedMeshInstances.empty()) {
        SkeletonComponent sc;
        sc.m_skeleton          = proto.m_skeleton;
        sc.m_animations        = proto.m_animations;
        sc.m_loop              = true;
        sc.m_speed             = 1.0f;
        sc.m_currentAnimation  = 0;

        // Pre-allocate the state buffers so the first frame has correctly sized
        // output spans for SamplingJob / LocalToModelJob.
        SkeletonStateComponent ssc;
        if (!proto.m_animations.empty()) {
            ssc.m_localTransforms.resize(
                static_cast<size_t>(proto.m_skeleton->num_soa_joints()));
            ssc.m_modelMatrices.resize(
                static_cast<size_t>(proto.m_skeleton->num_joints()));
            ssc.m_time = 0.0f;
        }

        skeletonEntity = en.CreateEntity();
        en.AddComponent(skeletonEntity, std::move(sc));
        en.AddComponent(skeletonEntity, std::move(ssc));
        en.AddComponent(skeletonEntity, Transform::Identity());
    }

    // ── Skinned mesh entities ───────────────────────────────────────────────
    // Resolve SkinData::m_skeletonJointIndices lazily here so it happens once
    // per skin at scene load time rather than every frame.
    if (proto.m_skeleton) {
        auto const& jointNames = proto.m_skeleton->joint_names();
        int numSkelJoints = proto.m_skeleton->num_joints();

        for (auto& skinDef : proto.m_skins) {
            auto& sd = *skinDef.m_skinData;
            sd.m_skeletonJointIndices.resize(sd.m_jointNames.size(), -1);
            for (size_t si = 0; si < sd.m_jointNames.size(); ++si) {
                for (int sj = 0; sj < numSkelJoints; ++sj) {
                    if (sd.m_jointNames[si] == jointNames[sj]) {
                        sd.m_skeletonJointIndices[si] = sj;
                        break;
                    }
                }
            }
        }
    }

    for (auto& inst : proto.m_skinnedMeshInstances) {
        auto geoH = en.CreateGeometry(std::move(inst.m_geometry));

        MaterialHandle matH;
        if (inst.m_materialIndex >= 0 &&
            inst.m_materialIndex < static_cast<int>(materials.size()))
            matH = materials[inst.m_materialIndex];

        std::shared_ptr<SkinData> skinData;
        if (inst.m_skinIndex >= 0 &&
            inst.m_skinIndex < static_cast<int>(proto.m_skins.size()))
            skinData = proto.m_skins[inst.m_skinIndex].m_skinData;

        auto world  = root * inst.m_worldTransform;
        auto entity = en.CreateEntity();
        en.AddComponent(entity, SkinnedMeshComponent{geoH, matH, skinData, skeletonEntity});
        en.AddComponent(entity, world);
    }
}

} // namespace okami
