#pragma once
#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

// Abstract base class for asset processors.
// Each subclass handles one type of input file (by extension) and knows how to
// transform it into the corresponding output file.
//
// The calling mechanism (asset_builder) is responsible for deciding whether
// processing is actually needed (e.g. by comparing file timestamps) and for
// resolving and supplying YAML settings via LoadSettings().
class AssetProcessor {
public:
    virtual ~AssetProcessor() = default;

    // Returns true if this processor can handle the given input file.
    virtual bool CanProcess(const std::filesystem::path& inputPath) const = 0;

    // Returns the output path for a given input path (relative to the asset
    // root).  Processors that change the file format should change the
    // extension here (e.g. .png -> .ktx2).  Processors that keep the same
    // format should return the input path unchanged.
    virtual std::filesystem::path OutputPath(const std::filesystem::path& inputRelPath) const = 0;

    // Performs the conversion from 'input' to 'output'.
    // Both paths are absolute.  The output directory is guaranteed to exist
    // before this is called.  Throws std::runtime_error on failure.
    virtual void Process(const std::filesystem::path& input,
                         const std::filesystem::path& output) = 0;

    // Called by asset_builder on directory enter with the contents of the
    // directory-wide settings file for this processor (e.g. "texture.yaml").
    // Also called just before Process() with the per-file settings yaml if one
    // exists.  Should copy the current top-of-stack, apply any overrides from
    // 'settings', and push the result.
    // If 'settings' is null/empty, just pushes a copy of the current top.
    virtual void PushSettings(const YAML::Node& settings) {}

    // Called by asset_builder on directory exit (matching each PushSettings
    // called on entry), and after Process() for a per-file yaml push.
    virtual void PopSettings() {}

    // Name of the directory-wide settings file for this processor type
    // (e.g. "texture.yaml").  Returns an empty string by default (no settings).
    virtual std::string SettingsFileName() const { return ""; }

    // Returns true if the output needs to be (re)generated from the input.
    // The default implementation compares file modification times.
    // Processors with additional dependencies (e.g. shader #includes) should
    // override this to account for them.
    virtual bool NeedsProcessing(const std::filesystem::path& input,
                                 const std::filesystem::path& output) const {
        if (!std::filesystem::exists(output))
            return true;
        return std::filesystem::last_write_time(input)
             > std::filesystem::last_write_time(output);
    }
};
