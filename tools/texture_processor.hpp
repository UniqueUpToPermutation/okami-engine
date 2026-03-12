#pragma once
#include "asset_processor.hpp"

#include <map>

// Per-asset settings for the texture processor.
// These can be supplied via a YAML file (see asset_builder for the lookup rules).
struct TextureProcessorParams {
    bool buildMips   = true;   // generate a full mip chain (true) or only level 0 (false)
    bool linearMips  = false;  // false = gamma-correct sRGB-aware filtering (default)
                               // true  = simple linear averaging (faster, less accurate)
    bool copySource  = false;  // also copy the original source file alongside the .ktx2
};

// Converts PNG / JPEG images to KTX2 format.
// Uses gamma-correct (sRGB-aware) box filtering for mip generation by default.
class TextureProcessor : public AssetProcessor {
public:
    // 'quiet'       - suppress per-file progress output.
    // 'initialParams' - params used when LoadSettings() is never called
    //                   (e.g. the standalone png2ktx tool).
    explicit TextureProcessor(bool quiet = false,
                              TextureProcessorParams initialParams = {});

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Changes the extension to .ktx2.
    std::filesystem::path OutputPath(const std::filesystem::path& inputRelPath) const override;

    void Process(const std::filesystem::path& input,
                 const std::filesystem::path& output) override;

    // Pushes a new settings frame derived from the current top.
    // Only keys present in 'settings' are overridden; absent keys inherit
    // the value from the frame below.
    void PushSettings(const YAML::Node& settings) override;
    void PopSettings() override;

    std::string SettingsFileName() const override { return "texture.yaml"; }

    // Compares the current effective settings (YAML stack + virtual override) against
    // the stored sidecar alongside 'output'.  Returns true if the output is missing,
    // the input is newer, or the settings have changed since the last build.
    bool NeedsProcessing(const std::filesystem::path& input,
                         const std::filesystem::path& output) const override;

    // Register a virtual override for a specific absolute input path.
    // It is applied on top of all YAML-file settings when that file is processed
    // and is what gets stored in the sidecar for future staleness comparisons.
    // Intended to be called by GeometryProcessor before the texture-processing pass
    // so that each GLTF-referenced texture uses the correct mip mode
    // (sRGB-aware for albedo/emissive, linear for normals/metallic-roughness).
    void SetVirtualOverride(const std::filesystem::path& absInputPath,
                            const YAML::Node& overrides);

private:
    bool m_quiet;
    std::vector<TextureProcessorParams>         m_stack;
    std::map<std::filesystem::path, YAML::Node> m_overrides;

    // Returns the effective params for 'input': stack top merged with any
    // virtual override registered for that path.
    // Must be called AFTER PushSettings has been applied for the current file.
    TextureProcessorParams EffectiveParams(const std::filesystem::path& input) const;

    // Path to the sidecar settings file alongside a given output file.
    // e.g. foo/bar.ktx2 -> foo/bar.ktx2.settings
    static std::filesystem::path SidecarPath(const std::filesystem::path& output);

    // Write 'params' as YAML to the sidecar file for 'output'.
    void WriteSidecar(const std::filesystem::path& output,
                      const TextureProcessorParams& params) const;
};
