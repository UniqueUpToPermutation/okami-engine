// Pull in the tinygltf implementation here — this is the only TU in AssetBuilder
// that includes tiny_gltf.h, so the implementation definition belongs here.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "geometry_processor.hpp"

#include <tiny_gltf.h>

// ozz offline pipeline — skeleton + animation conversion
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/transform.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/base/span.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------

namespace {

static std::string LowerExt(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

// Shared no-op image loader: we never want tinygltf to decode image data here.
static bool NoopImageLoader(
    tinygltf::Image*, const int, std::string*, std::string*,
    int, int, const unsigned char*, int, void*)
{ return true; }

// Load a GLTF/GLB file without decoding images.
// Throws std::runtime_error on failure.
static tinygltf::Model LoadGltfModel(const fs::path& absPath) {
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(NoopImageLoader, nullptr);

    tinygltf::Model model;
    std::string err, warn;
    bool ok = (LowerExt(absPath) == ".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, absPath.string())
        : loader.LoadASCIIFromFile(&model, &err, &warn, absPath.string());

    if (!ok || !err.empty())
        throw std::runtime_error("failed to parse GLTF '" + absPath.string() + "': " + err);
    return model;
}

// Replace every non-alphanumeric, non-underscore, non-hyphen character with '_'.
static std::string SanitizeName(const std::string& name) {
    std::string out = name;
    for (auto& c : out)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
            c = '_';
    return out;
}

// ============================================================
// ozz helpers — adapted from ozz/src/animation/offline/gltf/gltf2ozz.cc
// ============================================================

// Returns a typed view into a GLTF buffer via an accessor.
template <typename T>
static ozz::span<const T> BufferView(const tinygltf::Model& model,
                                     const tinygltf::Accessor& acc) {
    const int32_t component_size =
        tinygltf::GetComponentSizeInBytes(acc.componentType);
    const int32_t element_size =
        component_size * tinygltf::GetNumComponentsInType(acc.type);
    if (element_size != static_cast<int32_t>(sizeof(T)))
        return {};

    const tinygltf::BufferView& bv  = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer&     buf = model.buffers[bv.buffer];
    const T* begin = reinterpret_cast<const T*>(
        buf.data.data() + bv.byteOffset + acc.byteOffset);
    return ozz::span<const T>(begin, begin + acc.count);
}

template <typename _KT>
static bool SampleLinearChannel(const tinygltf::Model& model,
                                 const tinygltf::Accessor& output_acc,
                                 const ozz::span<const float>& times,
                                 _KT* keyframes) {
    if (output_acc.count == 0) { keyframes->clear(); return true; }
    typedef typename _KT::value_type::Value V;
    auto values = BufferView<V>(model, output_acc);
    if (values.size() != output_acc.count || times.size() != output_acc.count)
        return false;
    keyframes->reserve(output_acc.count);
    for (size_t i = 0; i < output_acc.count; ++i)
        keyframes->push_back({times[i], values[i]});
    return true;
}

template <typename _KT>
static bool SampleStepChannel(const tinygltf::Model& model,
                               const tinygltf::Accessor& output_acc,
                               const ozz::span<const float>& times,
                               _KT* keyframes) {
    if (output_acc.count == 0) { keyframes->clear(); return true; }
    typedef typename _KT::value_type::Value V;
    auto values = BufferView<V>(model, output_acc);
    if (values.size() != output_acc.count || times.size() != output_acc.count)
        return false;

    keyframes->resize(output_acc.count * 2 - 1);
    for (size_t i = 0; i < output_acc.count; ++i) {
        (*keyframes)[i * 2] = {times[i], values[i]};
        if (i < output_acc.count - 1)
            (*keyframes)[i * 2 + 1] = {std::nexttowardf(times[i + 1], 0.f), values[i]};
    }
    return true;
}

template <typename T>
static T SampleHermite(float a, const T& p0, const T& m0, const T& p1, const T& m1) {
    const float t2 = a * a, t3 = t2 * a;
    return p0 * (2*t3 - 3*t2 + 1) + m0 * (t3 - 2*t2 + a) +
           p1 * (-2*t3 + 3*t2)    + m1 * (t3 - t2);
}

template <typename _KT>
static bool SampleCubicSplineChannel(const tinygltf::Model& model,
                                      const tinygltf::Accessor& output_acc,
                                      const ozz::span<const float>& times,
                                      float sampling_rate, float /*duration*/,
                                      _KT* keyframes) {
    assert(output_acc.count % 3 == 0);
    const size_t n = output_acc.count / 3;
    if (n == 0) { keyframes->clear(); return true; }
    typedef typename _KT::value_type::Value V;
    auto values = BufferView<V>(model, output_acc);
    if (values.size() / 3 != n || times.size() != n) return false;

    ozz::animation::offline::FixedRateSamplingTime iter(
        times[n - 1] - times[0], sampling_rate);
    keyframes->resize(iter.num_keys());
    size_t ck = 0;
    for (size_t k = 0; k < iter.num_keys(); ++k) {
        const float t  = iter.time(k) + times[0];
        while (ck + 1 < n - 1 && times[ck + 1] < t) ++ck;
        const float t0 = times[ck], t1 = times[ck + 1];
        const float alpha = (t - t0) / (t1 - t0);
        const V& p0 = values[ck * 3 + 1];
        const V  m0 = values[ck * 3 + 2] * (t1 - t0);
        const V& p1 = values[(ck + 1) * 3 + 1];
        const V  m1 = values[(ck + 1) * 3] * (t1 - t0);
        (*keyframes)[k] = {t, SampleHermite(alpha, p0, m0, p1, m1)};
    }
    return true;
}

template <typename _KT>
static bool SampleChannel(const tinygltf::Model& model,
                           const std::string& interp,
                           const tinygltf::Accessor& output_acc,
                           const ozz::span<const float>& times,
                           float rate, float duration,
                           _KT* keyframes) {
    bool ok = false;
    if      (interp == "LINEAR")      ok = SampleLinearChannel(model, output_acc, times, keyframes);
    else if (interp == "STEP")        ok = SampleStepChannel(model, output_acc, times, keyframes);
    else if (interp == "CUBICSPLINE") ok = SampleCubicSplineChannel(model, output_acc, times, rate, duration, keyframes);
    else { std::cerr << "ozz: unknown interpolation '" << interp << "'\n"; return false; }

    if (ok) {
        ok = std::is_sorted(keyframes->begin(), keyframes->end(),
                            [](const auto& a, const auto& b){ return a.time < b.time; });
        if (!ok) { std::cerr << "ozz: keyframes not sorted.\n"; return false; }

        auto new_end = std::unique(keyframes->begin(), keyframes->end(),
                                   [](const auto& a, const auto& b){ return a.time == b.time; });
        keyframes->erase(new_end, keyframes->end());
    }
    return ok;
}

// Rest-pose key factories
static ozz::animation::offline::RawAnimation::TranslationKey
MakeTransKey(const tinygltf::Node& n) {
    if (n.translation.size() >= 3)
        return {0.f, {static_cast<float>(n.translation[0]),
                      static_cast<float>(n.translation[1]),
                      static_cast<float>(n.translation[2])}};
    return {0.f, ozz::math::Float3::zero()};
}
static ozz::animation::offline::RawAnimation::RotationKey
MakeRotKey(const tinygltf::Node& n) {
    if (n.rotation.size() >= 4)
        return {0.f, {static_cast<float>(n.rotation[0]),
                      static_cast<float>(n.rotation[1]),
                      static_cast<float>(n.rotation[2]),
                      static_cast<float>(n.rotation[3])}};
    return {0.f, ozz::math::Quaternion::identity()};
}
static ozz::animation::offline::RawAnimation::ScaleKey
MakeScaleKey(const tinygltf::Node& n) {
    if (n.scale.size() >= 3)
        return {0.f, {static_cast<float>(n.scale[0]),
                      static_cast<float>(n.scale[1]),
                      static_cast<float>(n.scale[2])}};
    return {0.f, ozz::math::Float3::one()};
}

static const tinygltf::Node* FindNodeByName(const tinygltf::Model& model,
                                             const std::string& name) {
    for (const auto& node : model.nodes)
        if (node.name == name) return &node;
    return nullptr;
}

static std::vector<tinygltf::Skin> GetSkinsForScene(const tinygltf::Model& model,
                                                     const tinygltf::Scene& scene) {
    std::unordered_set<int> visited;
    std::vector<int> open(scene.nodes.begin(), scene.nodes.end());
    while (!open.empty()) {
        int idx = open.back(); open.pop_back();
        if (!visited.insert(idx).second) continue;
        for (int c : model.nodes[static_cast<size_t>(idx)].children) open.push_back(c);
    }
    std::vector<tinygltf::Skin> skins;
    for (const auto& skin : model.skins)
        if (!skin.joints.empty() && visited.count(skin.joints[0]))
            skins.push_back(skin);
    return skins;
}

static void FindSkinRoots(const tinygltf::Model& model,
                           const std::vector<tinygltf::Skin>& skins,
                           std::vector<int>& roots) {
    constexpr int kNoParent = -1, kVisited = -2;
    std::vector<int> parents(model.nodes.size(), kNoParent);
    for (int i = 0; i < static_cast<int>(model.nodes.size()); ++i)
        for (int c : model.nodes[static_cast<size_t>(i)].children)
            parents[static_cast<size_t>(c)] = i;

    for (const auto& skin : skins) {
        if (skin.joints.empty()) continue;
        if (skin.skeleton != -1) {
            parents[static_cast<size_t>(skin.skeleton)] = kVisited;
            roots.push_back(skin.skeleton);
            continue;
        }
        int r = skin.joints[0];
        while (r != kVisited && parents[static_cast<size_t>(r)] != kNoParent)
            r = parents[static_cast<size_t>(r)];
        if (r != kVisited) roots.push_back(r);
    }
}

static bool ImportJointNode(const tinygltf::Model& model,
                             const tinygltf::Node& src,
                             ozz::animation::offline::RawSkeleton::Joint& dst) {
    dst.name = src.name.c_str();
    dst.transform = ozz::math::Transform::identity();

    if (!src.matrix.empty() && src.matrix.size() == 16) {
        ozz::math::Float4x4 m = {{
            ozz::math::simd_float4::Load(
                static_cast<float>(src.matrix[0]),  static_cast<float>(src.matrix[1]),
                static_cast<float>(src.matrix[2]),  static_cast<float>(src.matrix[3])),
            ozz::math::simd_float4::Load(
                static_cast<float>(src.matrix[4]),  static_cast<float>(src.matrix[5]),
                static_cast<float>(src.matrix[6]),  static_cast<float>(src.matrix[7])),
            ozz::math::simd_float4::Load(
                static_cast<float>(src.matrix[8]),  static_cast<float>(src.matrix[9]),
                static_cast<float>(src.matrix[10]), static_cast<float>(src.matrix[11])),
            ozz::math::simd_float4::Load(
                static_cast<float>(src.matrix[12]), static_cast<float>(src.matrix[13]),
                static_cast<float>(src.matrix[14]), static_cast<float>(src.matrix[15]))}};
        if (!ozz::math::ToAffine(m, &dst.transform)) {
            std::cerr << "ozz: failed to decompose matrix for node '"
                      << src.name << "'\n";
            return false;
        }
    } else {
        if (src.translation.size() >= 3)
            dst.transform.translation = {static_cast<float>(src.translation[0]),
                                         static_cast<float>(src.translation[1]),
                                         static_cast<float>(src.translation[2])};
        if (src.rotation.size() >= 4)
            dst.transform.rotation = {static_cast<float>(src.rotation[0]),
                                      static_cast<float>(src.rotation[1]),
                                      static_cast<float>(src.rotation[2]),
                                      static_cast<float>(src.rotation[3])};
        if (src.scale.size() >= 3)
            dst.transform.scale = {static_cast<float>(src.scale[0]),
                                   static_cast<float>(src.scale[1]),
                                   static_cast<float>(src.scale[2])};
    }

    dst.children.resize(src.children.size());
    for (size_t i = 0; i < src.children.size(); ++i)
        if (!ImportJointNode(model, model.nodes[static_cast<size_t>(src.children[i])], dst.children[i]))
            return false;
    return true;
}

// Build a RawSkeleton from the default scene of a GLTF model.
static bool BuildRawSkeleton(const tinygltf::Model& model,
                              ozz::animation::offline::RawSkeleton& out) {
    if (model.scenes.empty()) return false;
    const tinygltf::Scene& scene =
        model.scenes[static_cast<size_t>(model.defaultScene >= 0 ? model.defaultScene : 0)];
    if (scene.nodes.empty()) return false;

    auto skins = GetSkinsForScene(model, scene);
    std::vector<int> roots;
    if (skins.empty()) {
        roots.assign(scene.nodes.begin(), scene.nodes.end());
    } else {
        FindSkinRoots(model, skins, roots);
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

    out.roots.resize(roots.size());
    for (size_t i = 0; i < roots.size(); ++i)
        if (!ImportJointNode(model, model.nodes[static_cast<size_t>(roots[i])], out.roots[i]))
            return false;

    return out.Validate();
}

// Sample one animation channel into a joint track.
static bool SampleAnimChannel(
    const tinygltf::Model& model,
    const tinygltf::AnimationSampler& sampler,
    const std::string& target_path,
    float rate,
    float* duration,
    ozz::animation::offline::RawAnimation::JointTrack* track) {

    const auto& input_acc  = model.accessors[static_cast<size_t>(sampler.input)];
    const auto& output_acc = model.accessors[static_cast<size_t>(sampler.output)];
    assert(!input_acc.maxValues.empty());
    const float ch_dur = static_cast<float>(input_acc.maxValues[0]);
    if (ch_dur > *duration) *duration = ch_dur;

    auto timestamps = BufferView<float>(model, input_acc);
    if (timestamps.empty()) return true;

    if (target_path == "translation")
        return SampleChannel(model, sampler.interpolation, output_acc,
                             timestamps, rate, ch_dur, &track->translations);
    if (target_path == "rotation") {
        if (!SampleChannel(model, sampler.interpolation, output_acc,
                           timestamps, rate, ch_dur, &track->rotations))
            return false;
        for (auto& k : track->rotations)
            k.value = ozz::math::Normalize(k.value);
        return true;
    }
    if (target_path == "scale")
        return SampleChannel(model, sampler.interpolation, output_acc,
                             timestamps, rate, ch_dur, &track->scales);
    return false;
}

// Build a RawAnimation for the named clip, driven by a runtime Skeleton.
static bool BuildRawAnimation(
    const tinygltf::Model& model,
    const ozz::animation::Skeleton& skel,
    const std::string& anim_name,
    float rate,
    ozz::animation::offline::RawAnimation& out) {

    if (rate == 0.f) rate = 30.f;

    auto it = std::find_if(model.animations.begin(), model.animations.end(),
        [&anim_name](const tinygltf::Animation& a){ return a.name == anim_name; });
    if (it == model.animations.end()) {
        std::cerr << "ozz: animation '" << anim_name << "' not found.\n";
        return false;
    }
    const tinygltf::Animation& gltf_anim = *it;

    out.name     = gltf_anim.name.c_str();
    out.duration = 0.f;

    const int num_joints = skel.num_joints();
    out.tracks.resize(static_cast<size_t>(num_joints));

    // Map joint name -> channels in this animation
    std::unordered_map<std::string, std::vector<const tinygltf::AnimationChannel*>> cmap;
    for (const auto& ch : gltf_anim.channels) {
        if (ch.target_node < 0) continue;
        if (ch.target_path != "translation" && ch.target_path != "rotation" &&
            ch.target_path != "scale") continue;
        cmap[model.nodes[static_cast<size_t>(ch.target_node)].name].push_back(&ch);
    }

    const auto joint_names = skel.joint_names();
    for (int i = 0; i < num_joints; ++i) {
        auto& track = out.tracks[static_cast<size_t>(i)];
        const std::string jname(joint_names[static_cast<size_t>(i)]);

        for (const auto* ch : cmap[jname]) {
            if (!SampleAnimChannel(model,
                                   gltf_anim.samplers[static_cast<size_t>(ch->sampler)],
                                   ch->target_path, rate, &out.duration, &track))
                return false;
        }

        // Pad rest pose for channels that have no data in this clip
        const tinygltf::Node* node = FindNodeByName(model, jname);
        if (node) {
            if (track.translations.empty()) track.translations.push_back(MakeTransKey(*node));
            if (track.rotations.empty())    track.rotations.push_back(MakeRotKey(*node));
            if (track.scales.empty())       track.scales.push_back(MakeScaleKey(*node));
        }
    }

    if (!out.Validate()) {
        std::cerr << "ozz: animation '" << anim_name << "' failed validation.\n";
        return false;
    }
    return true;
}

// Serialize the skeleton from a GLTF model to an ozz binary file.
static void ExportOzzSkeleton(const tinygltf::Model& model,
                               const fs::path& output) {
    ozz::animation::offline::RawSkeleton raw;
    if (!BuildRawSkeleton(model, raw))
        throw std::runtime_error("ozz: failed to build skeleton");

    ozz::animation::offline::SkeletonBuilder builder;
    auto skel = builder(raw);
    if (!skel)
        throw std::runtime_error("ozz: SkeletonBuilder returned null");

    ozz::io::File file(output.string().c_str(), "wb");
    if (!file.opened())
        throw std::runtime_error("ozz: cannot open '" + output.string() + "' for writing");
    ozz::io::OArchive archive(&file);
    archive << *skel;
}

// Serialize one named animation clip from a GLTF model to an ozz binary file.
static void ExportOzzAnimation(const tinygltf::Model& model,
                                const std::string& anim_name,
                                float sampling_rate,
                                const fs::path& output) {
    // Build the runtime skeleton (needed for joint count / names).
    ozz::animation::offline::RawSkeleton raw_skel;
    if (!BuildRawSkeleton(model, raw_skel))
        throw std::runtime_error("ozz: failed to build skeleton for animation '" + anim_name + "'");

    ozz::animation::offline::SkeletonBuilder skel_builder;
    auto skel = skel_builder(raw_skel);
    if (!skel)
        throw std::runtime_error("ozz: SkeletonBuilder returned null");

    ozz::animation::offline::RawAnimation raw_anim;
    if (!BuildRawAnimation(model, *skel, anim_name, sampling_rate, raw_anim))
        throw std::runtime_error("ozz: failed to build animation '" + anim_name + "'");

    ozz::animation::offline::AnimationBuilder anim_builder;
    auto anim = anim_builder(raw_anim);
    if (!anim)
        throw std::runtime_error("ozz: AnimationBuilder returned null for '" + anim_name + "'");

    ozz::io::File file(output.string().c_str(), "wb");
    if (!file.opened())
        throw std::runtime_error("ozz: cannot open '" + output.string() + "' for writing");
    ozz::io::OArchive archive(&file);
    archive << *anim;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

GeometryProcessor::GeometryProcessor(bool quiet)
    : m_quiet(quiet) {}

bool GeometryProcessor::CanProcess(const fs::path& inputPath) const {
    auto ext = inputPath.extension();
    return ext == ".glb" || ext == ".gltf" || ext == ".bin";
}

void GeometryProcessor::BuildNodes(ResourceGraph& graph,
                                    NodeId inputNodeId,
                                    const fs::path& inputRelPath) {
    // One output node copying the file unchanged.
    ResourceNode out;
    out.outputFile    = inputRelPath;
    out.processorType = TypeName();
    NodeId outId = graph.AddNode(std::move(out));
    graph.AddEdge(inputNodeId, outId);

    // .bin files carry only raw vertex/index data — nothing to parse.
    if (LowerExt(inputRelPath) == ".bin")
        return;

    // Parse the GLTF/GLB to discover texture references and animation clips.
    fs::path absInput = graph.InputRoot() / inputRelPath;

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(NoopImageLoader, nullptr);

    tinygltf::Model model;
    std::string err, warn;
    bool ok = (LowerExt(inputRelPath) == ".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, absInput.string())
        : loader.LoadASCIIFromFile(&model, &err, &warn, absInput.string());

    if (!ok || !err.empty()) {
        std::cerr << "Warning [geometry]: could not parse "
                  << absInput.filename() << ": " << err << "\n";
        return;
    }

    // Classify each image index by semantic role.
    // linear wins if the image appears in both roles.
    std::unordered_set<int> srgbImages, linearImages;

    auto resolveImg = [&](int texIdx) -> int {
        if (texIdx < 0 || texIdx >= static_cast<int>(model.textures.size())) return -1;
        int src = model.textures[texIdx].source;
        if (src < 0 || src >= static_cast<int>(model.images.size())) return -1;
        return src;
    };

    for (const auto& mat : model.materials) {
        if (int i = resolveImg(mat.pbrMetallicRoughness.baseColorTexture.index); i >= 0)
            srgbImages.insert(i);
        if (int i = resolveImg(mat.emissiveTexture.index); i >= 0)
            srgbImages.insert(i);
        if (int i = resolveImg(mat.normalTexture.index); i >= 0)
            linearImages.insert(i);
        if (int i = resolveImg(mat.pbrMetallicRoughness.metallicRoughnessTexture.index); i >= 0)
            linearImages.insert(i);
        if (int i = resolveImg(mat.occlusionTexture.index); i >= 0)
            linearImages.insert(i);
    }

    // Wire a virtual config node for each texture, overriding linear_mips.
    fs::path absInputDir = absInput.parent_path();
    for (int i = 0; i < static_cast<int>(model.images.size()); ++i) {
        const auto& img = model.images[i];
        if (img.uri.empty()) continue; // embedded image — not in the file pipeline

        fs::path absTexture = absInputDir / img.uri;
        std::error_code ec;
        fs::path relTexture = fs::relative(absTexture, graph.InputRoot(), ec);
        if (ec || relTexture.empty()) continue;

        NodeId texSrcId = graph.FindByInput(relTexture);
        if (texSrcId == kInvalidNode) continue; // texture not in graph

        bool useLinear = linearImages.count(i) > 0;

        YAML::Node cfg;
        cfg["linear_mips"] = useLinear;

        // Give the virtual config node a deterministic synthetic path so
        // LoadCache can match it across runs.  The ".virtual.yaml" suffix
        // makes it explicit that this file does not exist on disk.
        fs::path cfgVirtualPath = relTexture.parent_path()
            / (relTexture.filename().string() + ".virtual.yaml");

        // Reuse an existing config node for this texture if one already exists
        // (e.g. recreated by LoadCache+BuildNodes on second run).
        NodeId cfgId = graph.FindByOutput(cfgVirtualPath);
        if (cfgId == kInvalidNode) {
            ResourceNode cfgNode;
            cfgNode.outputFile = cfgVirtualPath;
            cfgNode.config     = cfg;
            cfgId = graph.AddNode(std::move(cfgNode));
        } else {
            graph.GetNode(cfgId).config = cfg;
        }
        // Wire the GLB source as a dep of the config node so that changes to
        // the GLB (which may reclassify textures as linear/sRGB) advance the
        // config node's timestamp, making any downstream texture output stale.
        graph.AddEdge(inputNodeId, cfgId);
        graph.AddEdge(cfgId, texSrcId);
    }

    // -----------------------------------------------------------------------
    // Ozz animation / skeleton output nodes
    //
    // If the GLTF contains any animation clips, emit:
    //   {stem}.skeleton.ozz              — one shared skeleton
    //   {stem}.{sanitized_name}.animation.ozz  — one per animation clip
    // -----------------------------------------------------------------------
    if (!model.animations.empty()) {
        fs::path stem = inputRelPath.parent_path() / inputRelPath.stem();
        const std::string stemStr = stem.generic_string();

        // Skeleton node
        fs::path skelPath(stemStr + ".skeleton.ozz");
        NodeId skelId = graph.FindByOutput(skelPath);
        if (skelId == kInvalidNode) {
            ResourceNode skelNode;
            skelNode.outputFile    = skelPath;
            skelNode.processorType = TypeName();
            YAML::Node cfg;
            cfg["ozz_type"] = "skeleton";
            skelNode.config = cfg;
            skelId = graph.AddNode(std::move(skelNode));
        } else {
            graph.GetNode(skelId).config["ozz_type"] = "skeleton";
        }
        graph.AddEdge(inputNodeId, skelId);

        // One animation node per clip — deduplicate sanitized names with an index suffix.
        std::unordered_map<std::string, int> usedNames;
        for (const auto& anim : model.animations) {
            std::string safeName = SanitizeName(anim.name);
            int count = usedNames[safeName]++;
            if (count > 0) safeName += "_" + std::to_string(count);

            fs::path animPath(stemStr + "." + safeName + ".animation.ozz");
            NodeId animId = graph.FindByOutput(animPath);
            if (animId == kInvalidNode) {
                ResourceNode animNode;
                animNode.outputFile    = animPath;
                animNode.processorType = TypeName();
                YAML::Node cfg;
                cfg["ozz_type"]       = "animation";
                cfg["animation_name"] = anim.name;
                animNode.config = cfg;
                animId = graph.AddNode(std::move(animNode));
            } else {
                graph.GetNode(animId).config["ozz_type"]       = "animation";
                graph.GetNode(animId).config["animation_name"] = anim.name;
            }
            graph.AddEdge(inputNodeId, animId);
        }
    }
}

void GeometryProcessor::Process(ResourceGraph& graph, ResourceNode& node) {
    fs::path output = graph.AbsoluteOutputPath(node);

    // Non-ozz outputs.
    if (LowerExt(*node.outputFile) != ".ozz") {
        fs::path input = graph.AbsoluteSourceInputPath(node);
        if (LowerExt(*node.outputFile) == ".gltf") {
            // Rewrite the GLTF with texture URIs remapped to .ktx2.
            tinygltf::Model model = LoadGltfModel(input);
            for (auto& img : model.images) {
                if (!img.uri.empty())
                    img.uri = fs::path(img.uri).replace_extension(".ktx2").generic_string();
            }
            tinygltf::TinyGLTF writer;
            writer.SetImageLoader(NoopImageLoader, nullptr);
            if (!writer.WriteGltfSceneToFile(&model, output.string(),
                                              /*embedImages*/ false, /*embedBuffers*/ false,
                                              /*prettyPrint*/ false, /*writeBinary*/ false))
                throw std::runtime_error("geometry: failed to write GLTF '" + output.string() + "'");
        } else {
            fs::copy_file(input, output, fs::copy_options::overwrite_existing);
        }
        if (!m_quiet)
            std::cout << "geometry: " << input.filename().string()
                      << " -> " << output.filename().string() << "\n";
        return;
    }

    // Ozz outputs: re-parse the source GLTF and build the requested artifact.
    fs::path absGltf  = graph.AbsoluteSourceInputPath(node);
    tinygltf::Model model = LoadGltfModel(absGltf);

    const std::string ozzType = node.config["ozz_type"].as<std::string>();

    if (ozzType == "skeleton") {
        ExportOzzSkeleton(model, output);
        if (!m_quiet)
            std::cout << "geometry: " << absGltf.filename().string()
                      << " -> " << output.filename().string() << " [skeleton]\n";
    } else if (ozzType == "animation") {
        const std::string animName = node.config["animation_name"].as<std::string>();
        const float rate = node.config["sampling_rate"]
                               ? node.config["sampling_rate"].as<float>() : 0.f;
        ExportOzzAnimation(model, animName, rate, output);
        if (!m_quiet)
            std::cout << "geometry: " << absGltf.filename().string()
                      << " -> " << output.filename().string()
                      << " [animation: " << animName << "]\n";
    } else {
        throw std::runtime_error("geometry: unknown ozz_type '" + ozzType + "'");
    }
}

