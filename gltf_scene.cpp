#include "gltf_scene.hpp"

#include "material.hpp"   // BasicTexturedMaterial, DefaultMaterial

#include <tiny_gltf.h>

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
static Geometry BuildPrimitiveGeometry(
    tinygltf::Model   const& model,
    tinygltf::Primitive const& prim)
{
    std::vector<uint8_t>  packed;
    GeometryPrimitiveDesc primDesc;
    primDesc.m_type = MeshType::Static;

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

// Recursively walks the GLTF node tree and appends one GltfSceneMeshInstance
// per mesh primitive to 'out', using the accumulated world transform.
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
            for (auto const& prim : model.meshes[node.mesh].primitives) {
                GltfSceneMeshInstance inst;
                inst.m_worldTransform = world;
                inst.m_geometry       = BuildPrimitiveGeometry(model, prim);
                inst.m_materialIndex  = prim.material;   // may be -1
                out.m_meshInstances.push_back(std::move(inst));
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

        int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIdx >= 0 && texIdx < static_cast<int>(model.textures.size())) {
            int imgIdx = model.textures[texIdx].source;
            if (imgIdx >= 0 && imgIdx < static_cast<int>(model.images.size())) {
                auto const& img = model.images[imgIdx];
                if (!img.uri.empty()) {
                    std::filesystem::path texPath = basePath / img.uri;
                    if (params.m_useKtx2)
                        texPath.replace_extension(".ktx2");
                    def.m_colorTexturePath = std::move(texPath);
                }
            }
        }

        auto const& cf = mat.pbrMetallicRoughness.baseColorFactor;
        if (cf.size() == 4)
            def.m_colorTint = glm::vec4(cf[0], cf[1], cf[2], cf[3]);

        desc.m_materials.push_back(std::move(def));
    }

    // ── Scene graph ────────────────────────────────────────────────────────
    int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIdx < static_cast<int>(model.scenes.size()))
        BuildNodes(model, model.scenes[sceneIdx].nodes, Transform::Identity(), desc);

    return GltfScene{std::move(desc)};
}

// ─────────────────────────────────────────────────────────────────────────────
// SpawnGltfScene
// ─────────────────────────────────────────────────────────────────────────────

void SpawnGltfScene(Engine& en, GltfSceneDesc&& proto, Transform const& root) {
    // Build one material handle per GLTF material.
    // Materials with a resolvable texture path use BasicTexturedMaterial;
    // those without fall back to DefaultMaterial.
    std::vector<MaterialHandle> materials;
    materials.reserve(proto.m_materials.size());
    for (auto const& matDef : proto.m_materials) {
        if (!matDef.m_colorTexturePath.empty() &&
            std::filesystem::exists(matDef.m_colorTexturePath)) {
            BasicTexturedMaterial bm;
            bm.m_colorTexture = en.LoadTexture(matDef.m_colorTexturePath);
            bm.m_colorTint    = matDef.m_colorTint;
            materials.push_back(en.CreateMaterial(std::move(bm)));
        } else {
            materials.push_back(en.CreateMaterial(DefaultMaterial{}));
        }
    }

    // Spawn one entity per mesh instance.
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
}

} // namespace okami
