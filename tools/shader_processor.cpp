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
    : m_baseDirectory(baseDirectory), m_quiet(quiet), m_stack({ShaderProcessorParams{}}) {}

bool ShaderAssetProcessor::CanProcess(
        const std::filesystem::path& inputPath) const {
    auto ext = inputPath.extension().string();
    for (auto& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".glsl" || ext == ".vs"   || ext == ".fs"   ||
           ext == ".gs"   || ext == ".ts"   || ext == ".vert" ||
           ext == ".frag" || ext == ".wgsl";
}

std::filesystem::path ShaderAssetProcessor::OutputPath(
        const std::filesystem::path& inputRelPath) const {
    return inputRelPath;  // same path, no extension change
}

void ShaderAssetProcessor::Process(const std::filesystem::path& input,
                                    const std::filesystem::path& output) {
    std::filesystem::path baseDir = m_baseDirectory.empty()
                                        ? input.parent_path()
                                        : m_baseDirectory;
    ShaderPreprocessor preprocessor(baseDir);
    preprocessor.PreprocessFile(input, output);
    if (!m_quiet)
        std::cout << "shader: " << input.filename().string()
                  << " -> " << output.filename().string() << "\n";
}

bool ShaderAssetProcessor::NeedsProcessing(
        const std::filesystem::path& input,
        const std::filesystem::path& output) const {
    if (!std::filesystem::exists(output))
        return true;
    auto outputTime = std::filesystem::last_write_time(output);
    if (std::filesystem::last_write_time(input) > outputTime)
        return true;
    // Check all transitive #include dependencies.
    std::filesystem::path baseDir = m_baseDirectory.empty()
                                        ? input.parent_path()
                                        : m_baseDirectory;
    try {
        ShaderPreprocessor scanner(baseDir);
        for (const auto& dep : scanner.ScanDependencies(input)) {
            std::filesystem::path depPath(dep);
            if (std::filesystem::exists(depPath) &&
                std::filesystem::last_write_time(depPath) > outputTime)
                return true;
        }
    } catch (...) {
        // If scanning fails (e.g. missing include), force reprocessing so the
        // error surfaces properly during Process().
        return true;
    }
    return false;
}

void ShaderAssetProcessor::PushSettings(const YAML::Node& settings) {
    ShaderProcessorParams top = m_stack.back();
    // No fields yet - reserved for future options.
    (void)settings;
    m_stack.push_back(top);
}

void ShaderAssetProcessor::PopSettings() {
    if (m_stack.size() > 1)
        m_stack.pop_back();
}
