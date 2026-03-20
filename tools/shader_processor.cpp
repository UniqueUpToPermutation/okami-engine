#include "shader_processor.hpp"

#include <fstream>
#include <iostream>
#include <regex>
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
        const std::filesystem::path& outputFile) {
    m_includedFiles.clear();

    std::string content = ProcessFile(inputFile);

    std::filesystem::create_directories(outputFile.parent_path());

    std::ofstream out(outputFile);
    if (!out.is_open())
        throw std::runtime_error("Could not create output file: "
                                 + outputFile.string());
    out << content;
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
    // Output keeps the same relative path (no extension change).
    ResourceNode out;
    out.outputFile    = inputRelPath;
    out.processorType = TypeName();
    NodeId outId = graph.AddNode(std::move(out));
    graph.AddEdge(inputNodeId, outId);

    // Scan transitive #include dependencies and wire each as a dependency of
    // the output node so that editing any included file triggers a rebuild.
    std::filesystem::path absInput = graph.InputRoot() / inputRelPath;
    std::filesystem::path baseDir  = m_baseDirectory.empty()
                                         ? absInput.parent_path()
                                         : m_baseDirectory;
    try {
        ShaderPreprocessor scanner(baseDir);
        for (const auto& depStr : scanner.ScanDependencies(absInput)) {
            std::filesystem::path depAbs(depStr);
            // Only wire deps that live inside the input tree.
            auto relDep = std::filesystem::relative(depAbs, graph.InputRoot());
            // relative() stays inside the tree when it doesn't start with "..".
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
    } catch (...) {
        // If scanning fails (missing include etc.) ignore — the error will
        // surface properly when Process() is called during Build().
    }
}

void ShaderAssetProcessor::Process(ResourceGraph& graph, ResourceNode& node) {
    std::filesystem::path input  = graph.AbsoluteSourceInputPath(node);
    std::filesystem::path output = graph.AbsoluteOutputPath(node);
    std::filesystem::path baseDir = m_baseDirectory.empty()
                                        ? input.parent_path()
                                        : m_baseDirectory;
    ShaderPreprocessor preprocessor(baseDir);
    preprocessor.PreprocessFile(input, output);
    if (!m_quiet)
        std::cout << "shader: " << input.filename().string()
                  << " -> " << output.filename().string() << "\n";
}
