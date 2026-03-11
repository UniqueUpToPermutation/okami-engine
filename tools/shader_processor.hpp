#pragma once
#include "asset_processor.hpp"
#include <filesystem>
#include <string>
#include <unordered_set>

// Resolves #include directives in GLSL / WGSL shader files and writes the
// fully inlined result to an output file.  Used both as a library by
// ShaderAssetProcessor and directly by the standalone shader_preprocessor tool.
class ShaderPreprocessor {
public:
    explicit ShaderPreprocessor(const std::filesystem::path& baseDirectory);

    // Process 'inputFile' and write the result to 'outputFile'.
    // Creates the output directory if it does not exist.
    // Throws std::runtime_error on failure.
    void PreprocessFile(const std::filesystem::path& inputFile,
                        const std::filesystem::path& outputFile);

    // Collect the transitive set of files #included by 'inputFile' (not
    // including the file itself).  Does not write any output.
    // Throws std::runtime_error if an include cannot be resolved.
    std::unordered_set<std::string> ScanDependencies(
        const std::filesystem::path& inputFile);

private:
    std::unordered_set<std::string> m_includedFiles;
    std::filesystem::path m_baseDirectory;

    std::string ProcessFile(const std::filesystem::path& filePath);

    bool ProcessIncludeLine(const std::string& line,
                            const std::filesystem::path& currentDirectory,
                            std::string& result);
};

// ---------------------------------------------------------------------------
// AssetProcessor subclass
// ---------------------------------------------------------------------------

// Per-asset settings for the shader preprocessor.
// Currently empty, but exists for forward-compatibility with future options.
struct ShaderProcessorParams {
    // reserved for future options
};

// Runs the shader preprocessor on GLSL / WGSL files.
// Handled extensions: .glsl .vs .fs .gs .ts .vert .frag .wgsl
class ShaderAssetProcessor : public AssetProcessor {
public:
    // baseDirectory: fallback directory used when an #include cannot be
    // resolved relative to the including file.  If empty, the input file's
    // parent directory is used as the fallback.
    explicit ShaderAssetProcessor(
        const std::filesystem::path& baseDirectory = {},
        bool quiet = false);

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Keeps the same extension (output shader has the same name as input).
    std::filesystem::path OutputPath(
        const std::filesystem::path& inputRelPath) const override;

    void Process(const std::filesystem::path& input,
                 const std::filesystem::path& output) override;

    // Pushes a new settings frame derived from the current top.
    void PushSettings(const YAML::Node& settings) override;
    void PopSettings() override;

    bool NeedsProcessing(const std::filesystem::path& input,
                         const std::filesystem::path& output) const override;

    std::string SettingsFileName() const override { return "shader.yaml"; }

private:
    std::filesystem::path m_baseDirectory;
    bool m_quiet;
    std::vector<ShaderProcessorParams> m_stack;
};
