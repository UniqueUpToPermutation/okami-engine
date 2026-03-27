#pragma once
#include "asset_processor.hpp"
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

// Resolves #include directives in GLSL / WGSL shader files and writes the
// fully inlined result to an output file.  Used both as a library by
// ShaderAssetProcessor and directly by the standalone shader_preprocessor tool.
class ShaderPreprocessor {
public:
    explicit ShaderPreprocessor(const std::filesystem::path& baseDirectory);

    // Process 'inputFile' and write the result to 'outputFile'.
    // If 'defines' is non-empty, each entry is emitted as a '#define' line
    // immediately after the '#version' directive (or before all content if
    // no '#version' is present).  Format: "NAME" or "NAME VALUE".
    // Creates the output directory if it does not exist.
    // Throws std::runtime_error on failure.
    void PreprocessFile(const std::filesystem::path&    inputFile,
                        const std::filesystem::path&    outputFile,
                        const std::vector<std::string>& defines = {});

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

// Runs the shader preprocessor on GLSL / WGSL files.
// Handled extensions: .glsl .vs .fs .gs .ts .vert .frag .wgsl
//
// BuildNodes() also scans #include dependencies and wires them as graph edges
// so that a stale header causes all shaders that include it to be rebuilt.
class ShaderAssetProcessor : public AssetProcessor {
public:
    // baseDirectory: fallback directory used when an #include cannot be
    // resolved relative to the including file.  If empty, the input file's
    // parent directory is used as the fallback.
    explicit ShaderAssetProcessor(
        const std::filesystem::path& baseDirectory = {},
        bool quiet = false);

    std::string TypeName() const override { return "shader"; }

    bool CanProcess(const std::filesystem::path& inputPath) const override;

    // Creates one output node per target defined in the resolved 'shader.yaml'
    // config (naming convention: stem.TARGET.ext).  If no targets are defined,
    // falls back to producing a single output with the same relative path.
    // Also scans transitive #include dependencies and adds each discovered
    // file as an additional dependency edge of every output node.
    void BuildNodes(ResourceGraph&               graph,
                    NodeId                       inputNodeId,
                    const std::filesystem::path& inputRelPath) override;

    void Process(ResourceGraph& graph, ResourceNode& node) override;

    std::string SettingsFileName() const override { return "shader.yaml"; }

private:
    std::filesystem::path m_baseDirectory;
    bool                  m_quiet;
};
