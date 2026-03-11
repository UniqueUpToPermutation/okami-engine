#pragma once
#include "asset_processor.hpp"

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

private:
    bool m_quiet;
    std::vector<TextureProcessorParams> m_stack;
};
