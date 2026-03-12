// asset_builder — batch asset processor
//
// Usage: asset_builder <input_dir> <output_dir> [--verbose|--quiet]
//
// Recursively walks <input_dir>, applies the appropriate AssetProcessor for
// each recognised file type, and writes the result under the same relative
// path inside <output_dir> (possibly with a changed extension, e.g. .png →
// .ktx2).  Unrecognised file types are silently ignored.
//
// Output modes (default: normal):
//   --verbose  print processed files AND up-to-date skips, plus summary
//   (normal)   print only processed files and summary
//   --quiet    suppress all output
//
// Processing is skipped for any output file that already exists and is at
// least as new as the corresponding input file.

#include "asset_processor.hpp"
#include "texture_processor.hpp"
#include "shader_processor.hpp"
#include "geometry_processor.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Merge all keys from 'overlay' into 'base', overwriting existing keys.
static void MergeYaml(YAML::Node& base, const YAML::Node& overlay) {
    if (!overlay || !overlay.IsMap()) return;
    for (const auto& kv : overlay)
        base[kv.first.Scalar()] = kv.second;
}

// Build the merged YAML settings node for a single input file.
//
// 'settingsFile' is the directory-wide filename (e.g. "texture.yaml").
// Precedence (lowest to highest):
//   1. <inputRoot>/settingsFile
//   2. <inputRoot>/<subdir>/settingsFile  (each level, closest wins)
//   3. <inputRoot>/<relDir>/<filename><ext>.yaml  (per-file override)
static YAML::Node ResolveSettings(const fs::path& inputRoot,
                                   const fs::path& fileRelPath,
                                   const std::string& settingsFile) {
    YAML::Node merged;
    if (settingsFile.empty())
        return merged;

    auto tryMerge = [&](const fs::path& dir) {
        fs::path p = dir / settingsFile;
        if (!fs::exists(p)) return;
        try {
            MergeYaml(merged, YAML::LoadFile(p.string()));
        } catch (const std::exception& e) {
            std::cerr << "Warning: could not parse " << p << ": " << e.what() << "\n";
        }
    };

    // Walk from root down to the file's parent directory.
    fs::path dir = inputRoot;
    tryMerge(dir);
    for (const auto& part : fileRelPath.parent_path()) {
        if (part == fs::path(".")) continue;
        dir /= part;
        tryMerge(dir);
    }

    // Per-file override: e.g. "diffuse.png.yaml"
    fs::path fileOverride = inputRoot / fileRelPath.parent_path() /
                            (fileRelPath.filename().string() + ".yaml");
    if (fs::exists(fileOverride)) {
        try {
            MergeYaml(merged, YAML::LoadFile(fileOverride.string()));
        } catch (const std::exception& e) {
            std::cerr << "Warning: could not parse " << fileOverride
                      << ": " << e.what() << "\n";
        }
    }

    return merged;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input_dir> <output_dir> [--quiet]\n";
        return 1;
    }

    bool quiet   = false;
    bool verbose  = false;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            std::cerr << "Unknown flag: " << argv[i] << "\n";
            return 1;
        }
    }

    fs::path inputRoot(argv[1]);
    fs::path outputRoot(argv[2]);

    if (!fs::exists(inputRoot) || !fs::is_directory(inputRoot)) {
        std::cerr << "Error: input directory does not exist: "
                  << inputRoot << "\n";
        return 1;
    }

    // Register processors in priority order.
    // The first processor whose CanProcess() returns true wins.
    //
    // TextureProcessor is also shared with GeometryProcessor so that
    // GLTF-referenced textures are imported with semantically-correct mip
    // settings (sRGB for albedo, linear for normal maps) before the standalone
    // texture pass runs.
    std::vector<std::unique_ptr<AssetProcessor>> processors;

    // Allocate TextureProcessor first and keep a raw reference for sharing.
    auto texProcOwned = std::make_unique<TextureProcessor>(/*quiet=*/quiet);
    TextureProcessor& texProcRef = *texProcOwned;
    processors.push_back(std::move(texProcOwned));

    processors.push_back(std::make_unique<ShaderAssetProcessor>(inputRoot, /*quiet=*/quiet));

    // GeometryProcessor borrows the TextureProcessor for embedded texture import.
    auto geoProcOwned = std::make_unique<GeometryProcessor>(texProcRef, /*quiet=*/quiet);
    GeometryProcessor* geoProcPtr = geoProcOwned.get();
    processors.push_back(std::move(geoProcOwned));

    int processed = 0;
    int skipped   = 0;
    int errors    = 0;

    // Pre-pass: parse every GLTF/GLB unconditionally to register virtual
    // overrides on the TextureProcessor.  This must happen before any
    // NeedsProcessing or Process calls so that the correct mip mode
    // (sRGB-aware vs linear) is known for each texture, even when the geometry
    // output file is already up-to-date and is therefore not re-copied.
    for (const auto& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& p = entry.path();
        auto ext = p.extension();
        if (ext == ".gltf" || ext == ".glb")
            geoProcPtr->RegisterOverrides(p);
    }

    // Main pass: process all assets, settings are pushed before NeedsProcessing
    // so sidecar comparison sees the fully resolved (override-inclusive) params.
    for (const auto& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (!entry.is_regular_file())
            continue;

        const fs::path& inputPath = entry.path();
        fs::path relPath = fs::relative(inputPath, inputRoot);

        if (inputPath.extension() == ".yaml")
            continue;

        AssetProcessor* proc = nullptr;
        for (auto& p : processors) {
            if (p->CanProcess(inputPath)) { proc = p.get(); break; }
        }
        if (!proc) continue;

        fs::path outputPath = outputRoot / proc->OutputPath(relPath);

        auto settings = ResolveSettings(inputRoot, relPath, proc->SettingsFileName());
        proc->PushSettings(settings);

        bool needsRebuild = proc->NeedsProcessing(inputPath, outputPath);

        if (!needsRebuild) {
            proc->PopSettings();
            if (verbose)
                std::cout << "  skip  " << relPath.string() << "\n";
            ++skipped;
            continue;
        }

        try {
            fs::create_directories(outputPath.parent_path());
        } catch (const std::exception& e) {
            proc->PopSettings();
            std::cerr << "Error creating directory "
                      << outputPath.parent_path() << ": " << e.what() << "\n";
            ++errors;
            continue;
        }

        try {
            proc->Process(inputPath, outputPath);
            ++processed;
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << relPath << ": "
                      << e.what() << "\n";
            ++errors;
        }
        proc->PopSettings();
    }

    if (!quiet) {
        std::cout << "\nDone: " << processed << " processed, "
                  << skipped << " up-to-date, "
                  << errors << " error(s).\n";
    }

    return errors > 0 ? 1 : 0;
}
