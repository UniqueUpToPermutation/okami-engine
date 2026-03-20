#pragma once
#include "asset_processor.hpp"

// Processes glTF / GLB geometry files:
//   - copies the .gltf/.glb/.bin file to the output unchanged
//   - for .gltf/.glb: parses the file and injects virtual config nodes into the
//     ResourceGraph for every referenced texture, overriding linear_mips based
//     on the texture's semantic role:
//       albedo / emissive  → linear_mips: false  (sRGB-aware mip filtering)
//       normal / metallic-roughness / occlusion → linear_mips: true (linear averaging)
//     These virtual config nodes are wired as dependencies of the texture's
//     source node so ResolveConfig() applies them with the highest precedence.
class GeometryProcessor : public AssetProcessor {
public:
    explicit GeometryProcessor(bool quiet = false);

    std::string TypeName() const override { return "geometry"; }

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Creates one output node (same path) and, for .gltf/.glb, injects
    // virtual linear_mips config nodes for all referenced textures.
    void BuildNodes(ResourceGraph&               graph,
                    NodeId                       inputNodeId,
                    const std::filesystem::path& inputRelPath) override;

    // Copies the geometry file to the output path unchanged.
    void Process(ResourceGraph& graph, ResourceNode& node) override;

private:
    bool m_quiet;
};
