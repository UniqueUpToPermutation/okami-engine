// Pull in the tinygltf implementation here — this is the only TU in AssetBuilder
// that includes tiny_gltf.h, so the implementation definition belongs here.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "geometry_processor.hpp"

#include <tiny_gltf.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------

GeometryProcessor::GeometryProcessor(TextureProcessor& texProc, bool quiet)
    : m_texProc(texProc), m_quiet(quiet) {}

bool GeometryProcessor::CanProcess(const fs::path& inputPath) const {
    auto ext = inputPath.extension();
    return ext == ".glb" || ext == ".gltf" || ext == ".bin";
}

fs::path GeometryProcessor::OutputPath(const fs::path& inputRelPath) const {
    return inputRelPath;
}

// ---------------------------------------------------------------------------

static std::string LowerExt(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

void GeometryProcessor::Process(const fs::path& input, const fs::path& output) {
    // Copy the geometry file unchanged, then register texture overrides.
    fs::copy_file(input, output, fs::copy_options::overwrite_existing);
    if (!m_quiet)
        std::cout << "geometry: " << input.filename().string()
                  << " -> " << output.filename().string() << "\n";

    // .bin files carry only raw vertex/index data — register nothing.
    auto ext = LowerExt(input);
    if (ext == ".gltf" || ext == ".glb")
        RegisterOverrides(input);
}

void GeometryProcessor::RegisterOverrides(const fs::path& input) {
    // Parse the GLTF and classify each referenced texture by semantic role,
    // then register a virtual override on the TextureProcessor so that the
    // texture pass uses the correct mip mode (sRGB-aware vs linear) for each file.
    // This is separated from Process() so it can be called unconditionally in a
    // pre-pass, even for GLTF files whose output copy is already up-to-date.
    auto ext = LowerExt(input);

    static constexpr auto kNoopLoader = [](
        tinygltf::Image*, const int, std::string*, std::string*,
        int, int, const unsigned char*, int, void*) -> bool { return true; };

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(+kNoopLoader, nullptr);

    tinygltf::Model model;
    std::string err, warn;
    bool ok = (ext == ".glb")
        ? loader.LoadBinaryFromFile(&model, &err, &warn, input.string())
        : loader.LoadASCIIFromFile(&model, &err, &warn, input.string());

    if (!ok || !err.empty()) {
        std::cerr << "Warning [geometry]: could not parse "
                  << input.filename() << " for texture classification: " << err << "\n";
        return;
    }

    fs::path inBase = input.parent_path();

    // Classify each image index by its semantic role.
    // An image may be referenced from multiple material slots; if it appears as
    // both sRGB (albedo/emissive) and linear (normal/metallic-roughness), linear
    // wins to prevent incorrect gamma-decoding of non-colour data.
    std::unordered_set<int> srgbImages, linearImages;

    auto resolveImg = [&](int texIdx) -> int {
        if (texIdx < 0 || texIdx >= static_cast<int>(model.textures.size())) return -1;
        int src = model.textures[texIdx].source;
        if (src < 0 || src >= static_cast<int>(model.images.size())) return -1;
        return src;
    };

    for (auto const& mat : model.materials) {
        if (int i = resolveImg(mat.pbrMetallicRoughness.baseColorTexture.index); i >= 0)
            srgbImages.insert(i);
        if (int i = resolveImg(mat.emissiveTexture.index); i >= 0)
            srgbImages.insert(i);
        if (int i = resolveImg(mat.normalTexture.index); i >= 0)
            linearImages.insert(i);
        if (int i = resolveImg(mat.pbrMetallicRoughness.metallicRoughnessTexture.index); i >= 0)
            linearImages.insert(i);
        if (int i = resolveImg(mat.occlusionTexture.index); i >= 0)
            linearImages.insert(i);
    }

    // Register a virtual override for every texture referenced by this GLTF.
    // We set linear_mips explicitly for both roles so a directory-wide
    // texture.yaml with linear_mips:true doesn't corrupt albedo mips.
    for (int i = 0; i < static_cast<int>(model.images.size()); ++i) {
        const auto& img = model.images[i];
        if (img.uri.empty())
            continue; // embedded (base64) image — not in the file pipeline

        fs::path texIn = inBase / img.uri;
        if (!fs::exists(texIn))
            continue;

        // linear wins if the image appears in both roles
        bool useLinear = linearImages.count(i) > 0;

        YAML::Node ovr;
        ovr["linear_mips"] = useLinear;
        m_texProc.SetVirtualOverride(texIn, ovr);
    }
}


