#pragma once
#include "asset_processor.hpp"
#include "texture_processor.hpp"

// Processes glTF / GLB geometry files:
//   - copies the .gltf/.glb/.bin file to the output unchanged
//   - parses .gltf/.glb to discover referenced textures and processes them via
//     the supplied TextureProcessor with semantically-correct mip settings:
//       albedo / emissive textures  → sRGB-aware (gamma-correct) mip filtering
//       normal / metallic-roughness / occlusion textures → linear mip averaging
//
// By processing GLTF-referenced textures here (before the standalone
// TextureProcessor pass), the TextureProcessor will find them already
// up-to-date and skip them, avoiding double-processing and wrong mip modes.
class GeometryProcessor : public AssetProcessor {
public:
    // texProc  – shared TextureProcessor used to convert referenced images.
    // quiet    – suppress per-file progress output.
    explicit GeometryProcessor(TextureProcessor& texProc, bool quiet = false);

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Keeps the same extension (.gltf, .glb, or .bin).
    std::filesystem::path OutputPath(const std::filesystem::path& inputRelPath) const override;

    void Process(const std::filesystem::path& input,
                 const std::filesystem::path& output) override;

    // Parses 'input' (.gltf/.glb) and registers virtual overrides on the shared
    // TextureProcessor for every texture referenced by that file.
    // Does NOT copy the geometry file — safe to call unconditionally in a pre-pass
    // so that overrides are populated even when the geometry output is up-to-date.
    void RegisterOverrides(const std::filesystem::path& input);

private:
    TextureProcessor& m_texProc;
    bool              m_quiet;
};
