#include "shader_processor.hpp"

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

// ===========================================================================
// ShaderPreprocessor
// ===========================================================================

ShaderPreprocessor::ShaderPreprocessor(const std::filesystem::path& baseDirectory)
    : m_baseDirectory(baseDirectory) {}

std::string ShaderPreprocessor::ProcessFile(const std::filesystem::path& filePath) {
    auto absolutePath    = std::filesystem::absolute(filePath);
    std::string absStr   = absolutePath.string();

    if (m_includedFiles.count(absStr))
        return "";  // already included

    m_includedFiles.insert(absStr);

    std::ifstream file(filePath);
    if (!file.is_open())
        throw std::runtime_error("Could not open file: " + filePath.string());

    std::string result;
    std::string line;
    while (std::getline(file, line)) {
        if (ProcessIncludeLine(line, filePath.parent_path(), result)) {
            // consumed as an #include
        } else if (line.find("#pragma once") != std::string::npos) {
            result += "// #pragma once removed by preprocessor\n";
        } else {
            result += line + "\n";
        }
    }
    return result;
}

bool ShaderPreprocessor::ProcessIncludeLine(
        const std::string& line,
        const std::filesystem::path& currentDirectory,
        std::string& result) {
    static const std::regex includeRegex(
        R"(\s*#include\s+[\"<]([^\"<>]+)[\">]\s*)");
    std::smatch matches;

    if (!std::regex_match(line, matches, includeRegex))
        return false;

    if (line.find("// Preprocessor: Ignore") != std::string::npos) {
        result += line + "\n";
        return true;
    }

    std::string includeFile = matches[1].str();

    std::filesystem::path includePath;
    if (std::filesystem::path(includeFile).is_absolute()) {
        includePath = includeFile;
    } else {
        includePath = currentDirectory / includeFile;
    }

    if (!std::filesystem::exists(includePath)) {
        auto fallback = m_baseDirectory / includeFile;
        if (std::filesystem::exists(fallback)) {
            includePath = fallback;
        } else {
            throw std::runtime_error(
                "Include file not found: " + includeFile +
                " (searched in " + currentDirectory.string() +
                " and " + m_baseDirectory.string() + ")");
        }
    }

    result += "// #include \"" + includeFile + "\" - Start\n";
    result += ProcessFile(includePath);
    result += "// #include \"" + includeFile + "\" - End\n";
    return true;
}

void ShaderPreprocessor::PreprocessFile(
        const std::filesystem::path& inputFile,
        const std::filesystem::path& outputFile,
        const std::vector<std::string>& defines) {
    m_includedFiles.clear();

    std::string content = ProcessFile(inputFile);

    std::filesystem::create_directories(outputFile.parent_path());

    std::ofstream out(outputFile);
    if (!out.is_open())
        throw std::runtime_error("Could not create output file: "
                                 + outputFile.string());

    if (defines.empty()) {
        out << content;
        return;
    }

    // Build the #define block to inject immediately after the #version
    // directive (required to be first in GLSL).  If no #version is present
    // (e.g. WGSL, or a plain include-only file), prepend the defines.
    std::string defineBlock;
    for (const auto& def : defines)
        defineBlock += "#define " + def + "\n";

    std::istringstream ss(content);
    std::string line;
    std::string result;
    bool injected = false;
    while (std::getline(ss, line)) {
        result += line + "\n";
        if (!injected && line.find("#version") != std::string::npos) {
            result += defineBlock;
            injected = true;
        }
    }

    if (!injected)
        out << defineBlock;  // no #version — prepend
    out << result;
}

std::unordered_set<std::string> ShaderPreprocessor::ScanDependencies(
        const std::filesystem::path& inputFile) {
    m_includedFiles.clear();
    ProcessFile(inputFile);  // populates m_includedFiles
    // m_includedFiles contains inputFile itself — remove it so only deps remain
    auto deps = m_includedFiles;
    deps.erase(std::filesystem::absolute(inputFile).string());
    return deps;
}

// ===========================================================================
// ShaderAssetProcessor
// ===========================================================================

ShaderAssetProcessor::ShaderAssetProcessor(
        const std::filesystem::path& baseDirectory, bool quiet)
    : m_baseDirectory(baseDirectory), m_quiet(quiet) {}

bool ShaderAssetProcessor::CanProcess(
        const std::filesystem::path& inputPath) const {
    auto ext = inputPath.extension().string();
    for (auto& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".glsl" || ext == ".vs"   || ext == ".fs"   ||
           ext == ".gs"   || ext == ".ts"   || ext == ".vert" ||
           ext == ".frag" || ext == ".wgsl";
}

void ShaderAssetProcessor::BuildNodes(ResourceGraph& graph,
                                       NodeId inputNodeId,
                                       const std::filesystem::path& inputRelPath) {
    // Read the resolved config to determine if multi-target variants are
    // requested.  Config is already wired to inputNodeId by the asset_builder
    // from any shader.yaml files found in the directory hierarchy.
    YAML::Node config = graph.ResolveConfig(inputNodeId);

    std::vector<std::string> targets;
    if (config && config["targets"] && config["targets"].IsMap()) {
        for (const auto& entry : config["targets"])
            targets.push_back(entry.first.as<std::string>());
    }

    // Derive the set of output paths: one per target (stem.TARGET.ext) or
    // the input relative path unchanged when no targets are configured.
    std::vector<std::filesystem::path> outputPaths;
    if (targets.empty()) {
        outputPaths.push_back(inputRelPath);
    } else {
        auto stem = inputRelPath.stem().string();
        auto ext  = inputRelPath.extension().string();
        auto dir  = inputRelPath.parent_path();
        for (const auto& target : targets) {
            std::filesystem::path outName = stem + "." + target + ext;
            outputPaths.push_back(dir.empty() ? outName : dir / outName);
        }
    }

    // Scan transitive #include dependencies once (same for every variant).
    std::filesystem::path absInput = graph.InputRoot() / inputRelPath;
    std::filesystem::path baseDir  = m_baseDirectory.empty()
                                         ? absInput.parent_path()
                                         : m_baseDirectory;
    std::unordered_set<std::string> deps;
    try {
        ShaderPreprocessor scanner(baseDir);
        deps = scanner.ScanDependencies(absInput);
    } catch (...) {
        // Scanning failures surface properly at Process() time.
    }

    // Create one output node per variant and wire all edges.
    for (const auto& outRelPath : outputPaths) {
        ResourceNode out;
        out.outputFile    = outRelPath;
        out.processorType = TypeName();
        NodeId outId = graph.AddNode(std::move(out));
        graph.AddEdge(inputNodeId, outId);

        for (const auto& depStr : deps) {
            std::filesystem::path depAbs(depStr);
            auto relDep = std::filesystem::relative(depAbs, graph.InputRoot());
            if (relDep.empty() || relDep.native().find(
                    std::filesystem::path("..").native()) == 0)
                continue;

            NodeId depId = graph.FindByInput(relDep);
            if (depId == kInvalidNode) {
                ResourceNode depNode;
                depNode.inputFile = relDep;
                depId = graph.AddNode(std::move(depNode));
            }
            graph.AddEdge(depId, outId);
        }
    }
}

void ShaderAssetProcessor::Process(ResourceGraph& graph, ResourceNode& node) {
    std::filesystem::path input  = graph.AbsoluteSourceInputPath(node);
    std::filesystem::path output = graph.AbsoluteOutputPath(node);
    std::filesystem::path baseDir = m_baseDirectory.empty()
                                        ? input.parent_path()
                                        : m_baseDirectory;

    // Determine which target this node represents by comparing the output stem
    // against the source stem.  e.g. source "pbr.vert", output "pbr.skinned.vert"
    // → target "skinned".  Nodes without a target suffix use no extra defines.
    std::vector<std::string> defines;
    if (node.outputFile.has_value()) {
        std::string inStem  = input.stem().string();
        std::string outStem = node.outputFile->stem().string();
        if (outStem.size() > inStem.size() + 1 &&
            outStem.compare(0, inStem.size(), inStem) == 0 &&
            outStem[inStem.size()] == '.') {
            std::string targetName = outStem.substr(inStem.size() + 1);
            YAML::Node config = graph.ResolveConfig(node.id);
            if (config && config["targets"] && config["targets"][targetName]) {
                auto definesNode = config["targets"][targetName]["defines"];
                if (definesNode && definesNode.IsMap()) {
                    for (const auto& kv : definesNode) {
                        std::string name = kv.first.as<std::string>();
                        if (kv.second.IsNull())
                            defines.push_back(name);
                        else
                            defines.push_back(name + " " + kv.second.as<std::string>());
                    }
                }
            }
        }
    }

    ShaderPreprocessor preprocessor(baseDir);
    preprocessor.PreprocessFile(input, output, defines);
    if (!m_quiet)
        std::cout << "shader: " << input.filename().string()
                  << " -> " << output.filename().string() << "\n";
}
