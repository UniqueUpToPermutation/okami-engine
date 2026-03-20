// asset_builder — graph-based incremental asset processor
//
// Usage: asset_builder <input_dir> <output_dir> [--verbose|--quiet|--clean]
//
// Constructs a ResourceGraph representing every source file, its settings
// configs, and the output nodes produced by each AssetProcessor.  The graph
// is cached as YAML in the output directory; only stale nodes are rebuilt on
// subsequent runs.
//
// Processing pipeline per run
// ---------------------------
//  Pass 1  Create source-file marker nodes for every non-YAML input file.
//  Pass 2  Create file-backed config nodes from directory-level settings YAML
//          files (e.g. texture.yaml) and wire them as dependencies of every
//          source node in that directory subtree.
//  Pass 3  Create real file-backed config nodes (inputFile = YAML path) from
//          per-file YAML overrides (e.g. foo.png.yaml) and wire them to the
//          target source node.  Phase 1 in Build() advances their timestamp
//          from mtime automatically.  ResolveConfig gives them higher
//          precedence than directory-level settings because their inputFile
//          stem has a non-empty extension (e.g. "foo.png.yaml" → "foo.png").
//  Pass 4  For each source node, call the matching processor's BuildNodes()
//          to add output nodes and dependency edges.
//  Build   ResourceGraph::Build() propagates staleness and dispatches stale
//          output nodes to their processor's Process() method.
//  Cache   SaveCache() persists updated timestamps for the next run.

#include "asset_graph.hpp"
#include "asset_processor.hpp"
#include "texture_processor.hpp"
#include "shader_processor.hpp"
#include "geometry_processor.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input_dir> <output_dir> [--verbose|--quiet|--clean]\n";
        return 1;
    }

    bool quiet   = false;
    bool verbose = false;
    bool clean   = false;
    for (int i = 3; i < argc; ++i) {
        if      (std::strcmp(argv[i], "--quiet")   == 0) quiet   = true;
        else if (std::strcmp(argv[i], "--verbose") == 0) verbose = true;
        else if (std::strcmp(argv[i], "--clean")   == 0) clean   = true;
        else {
            std::cerr << "Unknown flag: " << argv[i] << "\n";
            return 1;
        }
    }

    fs::path inputRoot  = fs::absolute(argv[1]);
    fs::path outputRoot = fs::absolute(argv[2]);

    if (!fs::exists(inputRoot) || !fs::is_directory(inputRoot)) {
        std::cerr << "Error: input directory does not exist: " << inputRoot << "\n";
        return 1;
    }

    if (clean && fs::exists(outputRoot)) {
        if (!quiet)
            std::cout << "Cleaning output directory: " << outputRoot << "\n";
        fs::remove_all(outputRoot);
    }

    // -----------------------------------------------------------------------
    // Processor registration
    // -----------------------------------------------------------------------

    auto texProcOwned  = std::make_unique<TextureProcessor>(/*quiet=*/quiet);
    auto shaderProc    = std::make_unique<ShaderAssetProcessor>(inputRoot, /*quiet=*/quiet);
    auto geoProcOwned  = std::make_unique<GeometryProcessor>(/*quiet=*/quiet);

    // Ordered list used for CanProcess dispatch — first match wins.
    std::vector<AssetProcessor*> processors = {
        geoProcOwned.get(),   // geometry before texture so it owns .gltf/.glb
        texProcOwned.get(),
        shaderProc.get(),
    };

    // Map: settings-filename → processor (e.g. "texture.yaml" → TextureProcessor)
    std::unordered_map<std::string, AssetProcessor*> settingsFileMap;
    for (AssetProcessor* p : processors) {
        std::string sf = p->SettingsFileName();
        if (!sf.empty())
            settingsFileMap[sf] = p;
    }

    // -----------------------------------------------------------------------
    // Collect orphaned output paths from the previous cache BEFORE building
    // the new graph.  Any output path in the old cache that has no matching
    // node after Pass 4 means its source file was deleted; we will delete the
    // corresponding output file and it will not be written back to the cache.
    // -----------------------------------------------------------------------

    std::unordered_set<std::string> cachedOutputPaths;
    {
        fs::path cacheFile = outputRoot / ResourceGraph::kCacheFileName;
        if (fs::exists(cacheFile)) {
            try {
                YAML::Node root = YAML::LoadFile(cacheFile.string());
                if (root && root.IsSequence()) {
                    for (const auto& entry : root) {
                        if (entry["outputFile"])
                            cachedOutputPaths.insert(
                                entry["outputFile"].as<std::string>());
                    }
                }
            } catch (...) {}
        }
    }

    ResourceGraph graph(inputRoot, outputRoot);

    for (AssetProcessor* p : processors)
        graph.RegisterProcessor(p);

    // LoadCache() is called after Pass 4 so that all nodes (including output
    // nodes added by BuildNodes()) exist when the cache entries are matched.

    // -----------------------------------------------------------------------
    // Pass 1: source-file marker nodes
    //
    // For every regular non-YAML file in the input directory, create a
    // source-file marker node (processorType empty, no output).  We also
    // build a map from relative-directory-string → list of source NodeIds
    // in that subtree so pass 2 can wire config dependencies efficiently.
    // -----------------------------------------------------------------------

    // dirSourceNodes[relDirStr] = NodeIds of source files anywhere inside relDir
    std::unordered_map<std::string, std::vector<NodeId>> dirSourceNodes;

    for (const auto& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (!entry.is_regular_file()) continue;

        fs::path relPath = fs::relative(entry.path(), inputRoot);

        // Skip all YAML files (handled in passes 2 & 3).
        if (relPath.extension() == ".yaml") continue;

        NodeId id = graph.FindByInput(relPath);
        if (id == kInvalidNode) {
            ResourceNode node;
            node.inputFile = relPath;
            id = graph.AddNode(std::move(node));
        }

        // Register under every ancestor directory including root ("").
        fs::path dir = relPath.parent_path();
        while (true) {
            dirSourceNodes[dir.generic_string()].push_back(id);
            if (dir.empty()) break;
            dir = dir.parent_path();
        }
    }

    // -----------------------------------------------------------------------
    // Pass 2: directory-level settings config nodes (file-backed)
    //
    // For each registered settings YAML file found in the input tree, create
    // a file-backed config node (inputFile = relPath) so Build() can advance
    // its timestamp from mtime.  Wire it as a dependency of every source file
    // in the same directory subtree.  Shallower configs have fewer path
    // components; ResolveConfig() merges them shallower-first so deeper
    // directory settings override ancestor ones.
    // -----------------------------------------------------------------------

    for (const auto& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (!entry.is_regular_file()) continue;

        fs::path relPath = fs::relative(entry.path(), inputRoot);
        if (relPath.extension() != ".yaml") continue;

        auto sit = settingsFileMap.find(relPath.filename().string());
        if (sit == settingsFileMap.end()) continue;  // not a settings file

        YAML::Node cfg;
        try {
            cfg = YAML::LoadFile(entry.path().string());
        } catch (const std::exception& e) {
            std::cerr << "Warning: could not parse " << entry.path()
                      << ": " << e.what() << "\n";
            continue;
        }
        if (!cfg || !cfg.IsMap() || cfg.size() == 0) continue;

        // Config node lives at the directory level where the YAML was found.
        NodeId cfgId = graph.FindByInput(relPath);
        if (cfgId == kInvalidNode) {
            ResourceNode cfgNode;
            cfgNode.inputFile = relPath;
            cfgNode.config    = cfg;
            cfgId = graph.AddNode(std::move(cfgNode));
        }

        // Wire to all source nodes in this directory's subtree.
        std::string dirStr = relPath.parent_path().generic_string();
        auto dit = dirSourceNodes.find(dirStr);
        if (dit != dirSourceNodes.end()) {
            for (NodeId srcId : dit->second)
                graph.AddEdge(cfgId, srcId);
        }
    }

    // -----------------------------------------------------------------------
    // Pass 3: per-file YAML override config nodes (real file-backed nodes)
    //
    // Files named "<asset>.<ext>.yaml" are per-file config overrides for the
    // asset "<asset>.<ext>".  They become real file-backed nodes (inputFile =
    // relPath) so Phase 1 in Build() advances their timestamp automatically
    // from mtime — no manual FileMtime call needed here.
    //
    // ResolveConfig gives them higher precedence than directory-level settings
    // (like "texture.yaml") because their inputFile stem has a non-empty
    // extension (e.g. "test2.png.yaml" → stem "test2.png" → ext ".png").
    // -----------------------------------------------------------------------

    for (const auto& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (!entry.is_regular_file()) continue;

        fs::path relPath = fs::relative(entry.path(), inputRoot);
        if (relPath.extension() != ".yaml") continue;
        if (settingsFileMap.count(relPath.filename().string())) continue;

        // Strip the trailing ".yaml" to find the target asset path.
        fs::path targetRel = relPath.parent_path() / relPath.stem();
        NodeId targetId = graph.FindByInput(targetRel);
        if (targetId == kInvalidNode) continue;

        YAML::Node cfg;
        try {
            cfg = YAML::LoadFile(entry.path().string());
        } catch (const std::exception& e) {
            std::cerr << "Warning: could not parse " << entry.path()
                      << ": " << e.what() << "\n";
            continue;
        }
        if (!cfg || !cfg.IsMap() || cfg.size() == 0) continue;

        // Real file-backed node: inputFile lets Phase 1 pick up mtime changes.
        NodeId ovId = graph.FindByInput(relPath);
        if (ovId == kInvalidNode) {
            ResourceNode overrideNode;
            overrideNode.inputFile = relPath;
            overrideNode.config    = cfg;
            ovId = graph.AddNode(std::move(overrideNode));
        } else {
            graph.GetNode(ovId).config = cfg;
        }
        graph.AddEdge(ovId, targetId);
    }

    // -----------------------------------------------------------------------
    // Pass 4: let each processor add output nodes and edges (BuildNodes)
    //
    // Iterate over the source-file nodes created in pass 1.  snapshot the
    // count first so we don't visit nodes added by BuildNodes() itself.
    // -----------------------------------------------------------------------

    std::size_t pass1Count = graph.NodeCount();
    for (std::size_t i = 0; i < pass1Count; ++i) {
        const ResourceNode& node = graph.GetNode(i);
        if (!node.inputFile) continue;                     // config or virtual node
        if (!node.processorType.empty()) continue;         // already an output node

        fs::path absInput = graph.AbsoluteInputPath(node);
        // Copy the relative path before calling BuildNodes: AddNode() inside
        // BuildNodes can reallocate m_nodes, which would invalidate any
        // reference into the node (including *node.inputFile).
        fs::path inputRelPath = *node.inputFile;

        for (AssetProcessor* proc : processors) {
            if (!proc->CanProcess(absInput)) continue;
            proc->BuildNodes(graph, i, inputRelPath);
            break;  // first matching processor wins
        }
    }

    // Now that all nodes exist, load cached timestamps.
    graph.LoadCache();

    // -----------------------------------------------------------------------
    // Orphan cleanup: delete output files whose source no longer exists.
    // -----------------------------------------------------------------------

    for (const auto& relStr : cachedOutputPaths) {
        if (graph.FindByOutput(fs::path(relStr)) == kInvalidNode) {
            fs::path absOut = outputRoot / relStr;
            if (fs::exists(absOut)) {
                if (!quiet)
                    std::cout << "  remove " << relStr << "\n";
                std::error_code ec;
                fs::remove(absOut, ec);
                if (ec)
                    std::cerr << "Warning: could not remove orphaned output "
                              << absOut << ": " << ec.message() << "\n";
            }
        }
    }

    // -----------------------------------------------------------------------
    // Build and persist
    // -----------------------------------------------------------------------

    int exitCode = 0;
    try {
        graph.Build(verbose);
    } catch (const std::exception& e) {
        std::cerr << "Build error: " << e.what() << "\n";
        exitCode = 1;
    }

    // Always save cache so successfully-built nodes aren't reprocessed next run.
    try {
        graph.SaveCache();
    } catch (const std::exception& e) {
        std::cerr << "Warning: could not save asset graph cache: " << e.what() << "\n";
    }

    return exitCode;
}

