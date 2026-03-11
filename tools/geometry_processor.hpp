#pragma once
#include "asset_processor.hpp"

// Copies glTF / GLB geometry files from input to output unchanged.
// Intended as a passthrough that can be extended with real processing later.
class GeometryProcessor : public AssetProcessor {
public:
    explicit GeometryProcessor(bool quiet = false);

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Keeps the same extension (.gltf or .glb).
    std::filesystem::path OutputPath(const std::filesystem::path& inputRelPath) const override;

    void Process(const std::filesystem::path& input,
                 const std::filesystem::path& output) override;

private:
    bool m_quiet;
};
