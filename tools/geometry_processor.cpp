// Pull in the tinygltf implementation here — this is the only TU in AssetBuilder
// that includes tiny_gltf.h, so the implementation definition belongs here.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "geometry_processor.hpp"

#include <tiny_gltf.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------

GeometryProcessor::GeometryProcessor(bool quiet)
    : m_quiet(quiet) {}

bool GeometryProcessor::CanProcess(const fs::path& inputPath) const {
    auto ext = inputPath.extension();
    return ext == ".glb" || ext == ".gltf" || ext == ".bin";
}

// ---------------------------------------------------------------------------

static std::string LowerExt(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
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

    // Parse the GLTF/GLB to discover texture references and inject virtual
    // config nodes that set linear_mips correctly for each texture.
    fs::path absInput = graph.InputRoot() / inputRelPath;

    static constexpr auto kNoopLoader = [](
        tinygltf::Image*, const int, std::string*, std::string*,
        int, int, const unsigned char*, int, void*) -> bool { return true; };

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(+kNoopLoader, nullptr);

    tinygltf::Model model;
    std::string err, warn;
    bool ok = (LowerExt(inputRelPath) == ".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, absInput.string())
        : loader.LoadASCIIFromFile(&model, &err, &warn, absInput.string());

    if (!ok || !err.empty()) {
        std::cerr << "Warning [geometry]: could not parse "
                  << absInput.filename()
                  << " for texture classification: " << err << "\n";
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
}

void GeometryProcessor::Process(ResourceGraph& graph, ResourceNode& node) {
    fs::path input  = graph.AbsoluteSourceInputPath(node);
    fs::path output = graph.AbsoluteOutputPath(node);
    fs::copy_file(input, output, fs::copy_options::overwrite_existing);
    if (!m_quiet)
        std::cout << "geometry: " << input.filename().string()
                  << " -> " << output.filename().string() << "\n";
}

