#include "geometry_processor.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

GeometryProcessor::GeometryProcessor(bool quiet)
    : m_quiet(quiet) {}

bool GeometryProcessor::CanProcess(const fs::path& inputPath) const {
    auto ext = inputPath.extension();
    return ext == ".glb" || ext == ".gltf" || ext == ".bin";
}

fs::path GeometryProcessor::OutputPath(const fs::path& inputRelPath) const {
    return inputRelPath;
}

void GeometryProcessor::Process(const fs::path& input,
                                 const fs::path& output) {
    fs::copy_file(input, output, fs::copy_options::overwrite_existing);
    if (!m_quiet)
        std::cout << "geometry: " << input.filename().string()
                  << " -> " << output.filename().string() << "\n";
}
