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

// Return all YAML settings files that affect 'fileRelPath' (same walk order as
// ResolveSettings).  Only existing files are included.
static std::vector<fs::path> CollectSettingsFiles(const fs::path& inputRoot,
                                                   const fs::path& fileRelPath,
                                                   const std::string& settingsFile) {
    std::vector<fs::path> result;
    if (settingsFile.empty())
        return result;

    auto tryAdd = [&](const fs::path& dir) {
        fs::path p = dir / settingsFile;
        if (fs::exists(p)) result.push_back(p);
    };

    fs::path dir = inputRoot;
    tryAdd(dir);
    for (const auto& part : fileRelPath.parent_path()) {
        if (part == fs::path(".")) continue;
        dir /= part;
        tryAdd(dir);
    }

    fs::path fileOverride = inputRoot / fileRelPath.parent_path() /
                            (fileRelPath.filename().string() + ".yaml");
    if (fs::exists(fileOverride))
        result.push_back(fileOverride);

    return result;
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
    std::vector<std::unique_ptr<AssetProcessor>> processors;
    processors.push_back(std::make_unique<TextureProcessor>(/*quiet=*/quiet));
    processors.push_back(std::make_unique<ShaderAssetProcessor>(inputRoot, /*quiet=*/quiet));
    processors.push_back(std::make_unique<GeometryProcessor>(/*quiet=*/quiet));

    int processed = 0;
    int skipped   = 0;
    int errors    = 0;

    for (const auto& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (!entry.is_regular_file())
            continue;

        const fs::path& inputPath = entry.path();
        fs::path relPath = fs::relative(inputPath, inputRoot);

        // Skip YAML settings files — they are configuration, not assets.
        if (inputPath.extension() == ".yaml")
            continue;

        // Find a matching processor.
        AssetProcessor* proc = nullptr;
        for (auto& p : processors) {
            if (p->CanProcess(inputPath)) {
                proc = p.get();
                break;
            }
        }

        fs::path outputRelPath = proc ? proc->OutputPath(relPath) : relPath;
        fs::path outputPath    = outputRoot / outputRelPath;

        if (!proc) {
            // No processor claims this file — skip it silently.
            continue;
        }

        // Check whether the asset needs (re)processing: compare the processor's
        // own staleness check AND whether any YAML settings file is newer.
        auto settingsFiles = CollectSettingsFiles(inputRoot, relPath,
                                                  proc->SettingsFileName());
        bool needsRebuild = proc->NeedsProcessing(inputPath, outputPath);
        if (!needsRebuild && fs::exists(outputPath)) {
            auto outputTime = fs::last_write_time(outputPath);
            for (const auto& sf : settingsFiles) {
                if (fs::last_write_time(sf) > outputTime) {
                    needsRebuild = true;
                    break;
                }
            }
        }

        if (!needsRebuild) {
            if (verbose)
                std::cout << "  skip  " << relPath.string() << "\n";
            ++skipped;
            continue;
        }

        // Ensure the output directory exists.
        try {
            fs::create_directories(outputPath.parent_path());
        } catch (const std::exception& e) {
            std::cerr << "Error creating directory "
                      << outputPath.parent_path() << ": " << e.what() << "\n";
            ++errors;
            continue;
        }

        if (proc) {
            try {
                auto settings = ResolveSettings(inputRoot, relPath,
                                                proc->SettingsFileName());
                proc->PushSettings(settings);
                proc->Process(inputPath, outputPath);
                proc->PopSettings();
                ++processed;
            } catch (const std::exception& e) {
                std::cerr << "Error processing " << relPath << ": "
                          << e.what() << "\n";
                ++errors;
            }
        }
    }

    if (!quiet) {
        std::cout << "\nDone: " << processed << " processed, "
                  << skipped << " up-to-date, "
                  << errors << " error(s).\n";
    }

    return errors > 0 ? 1 : 0;
}
