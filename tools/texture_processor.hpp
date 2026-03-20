#pragma once
#include "asset_processor.hpp"

// Settings for the texture processor, populated from the resolved YAML config:
//   build_mips:  true  — generate a full mip chain (default)
//   linear_mips: false — sRGB-aware box filtering (default); true = linear averaging
//   copy_source: false — also copy the original source file alongside the .ktx2
struct TextureProcessorParams {
    bool buildMips  = true;
    bool linearMips = false;
    bool copySource = false;
};

// Converts PNG / JPEG images to KTX2 format.
// Uses gamma-correct (sRGB-aware) box filtering for mip generation by default.
class TextureProcessor : public AssetProcessor {
public:
    explicit TextureProcessor(bool quiet = false);

    std::string TypeName() const override { return "texture"; }

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Creates one output node (.ktx2) per accepted input and wires the edge.
    void BuildNodes(ResourceGraph&               graph,
                    NodeId                       inputNodeId,
                    const std::filesystem::path& inputRelPath) override;

    // Resolves settings via graph.ResolveConfig(node.id) then converts.
    void Process(ResourceGraph& graph, ResourceNode& node) override;

    std::string SettingsFileName() const override { return "texture.yaml"; }

    // Core conversion — also callable directly (e.g. from the png2ktx tool).
    static void ConvertTexture(const std::filesystem::path& input,
                               const std::filesystem::path& output,
                               const TextureProcessorParams& params);

    // Build a TextureProcessorParams from a resolved YAML::Node.
    // Keys: build_mips (bool), linear_mips (bool), copy_source (bool).
    static TextureProcessorParams ParamsFromConfig(const YAML::Node& cfg);

private:
    bool m_quiet;
};
