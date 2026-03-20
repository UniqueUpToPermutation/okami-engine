#pragma once

#include "asset_graph.hpp"

#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

// ---------------------------------------------------------------------------
// AssetProcessor — abstract base for all asset-processing passes.
//
// Role in the resource graph pipeline
// ------------------------------------
// Each subclass is responsible for one category of input file (identified by
// extension or content).  Its two jobs are:
//
//   1. BuildNodes()  — Called once per input file that CanProcess() accepts.
//                      The processor inspects the file, decides what output
//                      nodes it produces, and adds those nodes plus any
//                      dependency edges to the ResourceGraph.
//                      If the input node's cached timestamp is already equal
//                      to or later than the file's mtime on disk, the
//                      processor may skip the file entirely.
//                      Nodes that require work should have their processorType
//                      set to TypeName() so that Build() can dispatch them.
//
//   2. Process()     — Called by ResourceGraph::Build() when a node whose
//                      processorType matches TypeName() is determined to be
//                      stale.  Performs the actual conversion and writes the
//                      output file.  The output directory is guaranteed to
//                      exist before this is called.  Throws std::runtime_error
//                      on failure.  Call graph.ResolveConfig(node.id) to
//                      obtain the merged YAML configuration for the node.
//
// Configuration
// -------------
// Per-directory and per-file YAML settings are represented in the graph as
// config nodes — ResourceNodes with no inputFile that carry a YAML config.
// The asset_builder creates these nodes from SettingsFileName() files found
// while walking the input directory and wires them as dependencies of the
// input file nodes they apply to.  ResolveConfig() then merges them at
// process time: deeper (closer to the file) configs override shallower ones,
// and virtual config nodes (no inputFile) override everything.
// ---------------------------------------------------------------------------

class AssetProcessor {
public:
    virtual ~AssetProcessor() = default;

    // ------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------

    // Stable, unique identifier for this processor type.
    // Stored as ResourceNode::processorType in the graph cache.
    // Examples: "texture", "shader", "geometry".
    virtual std::string TypeName() const = 0;

    // Returns true if this processor can handle the given input file
    // (typically decided by extension).  'inputPath' is absolute.
    virtual bool CanProcess(const std::filesystem::path& inputPath) const = 0;

    // ------------------------------------------------------------------
    // Graph construction
    // ------------------------------------------------------------------

    // Inspect 'inputRelPath' (relative to the input-asset root) and add any
    // required output nodes and dependency edges to 'graph'.
    //
    // 'inputNodeId' is the pre-existing source-file marker node for this
    // input file (already in the graph with its cached timestamp loaded).
    // If that node's timestamp is >= the file's mtime, the file has not
    // changed since the last build and the processor may return immediately
    // without adding new nodes.
    //
    // Before this method is called, the asset_builder has already added config
    // nodes for every SettingsFileName() file found in the directory hierarchy
    // and wired them as dependencies of 'inputNodeId' (root-level config first,
    // then per-directory overrides, then any per-file YAML override).
    //
    // The processor is responsible for:
    //   - Creating output node(s) via graph.AddNode(), with processorType set
    //     to TypeName() and outputFile set to the relative output path.
    //   - Wiring edges with graph.AddEdge(inputNodeId, outputNodeId) and any
    //     additional inter-node edges (e.g. geometry -> texture).
    virtual void BuildNodes(ResourceGraph&               graph,
                            NodeId                       inputNodeId,
                            const std::filesystem::path& inputRelPath) = 0;

    // ------------------------------------------------------------------
    // Processing
    // ------------------------------------------------------------------

    // Perform the conversion described by 'node'.
    //   - graph.AbsoluteInputPath(node)  — source file (absolute).
    //   - graph.AbsoluteOutputPath(node) — destination file (absolute).
    //   - graph.ResolveConfig(node.id)   — merged YAML config (see below).
    //
    // Config resolution via graph.ResolveConfig(node.id):
    //   Walks all transitive dependency nodes that carry a non-empty config
    //   and merges them in ascending precedence order:
    //     1. File-backed config nodes (have inputFile), shallowest path first
    //        so that subdirectory configs override parent directory configs.
    //     2. Virtual config nodes (no inputFile) — highest precedence,
    //        applied last regardless of where they appear in the graph.
    //
    // The output directory is guaranteed to exist before this is called.
    // Throws std::runtime_error on failure.
    virtual void Process(ResourceGraph& graph, ResourceNode& node) = 0;

    // ------------------------------------------------------------------
    // Settings
    // ------------------------------------------------------------------

    // Name of the directory-wide settings YAML file for this processor
    // (e.g. "texture.yaml").  The asset_builder loads every file with this
    // name found while walking the input directory and inserts each as a
    // config node dependency of the input file nodes in that directory subtree.
    // Returns an empty string by default (no directory-wide settings).
    virtual std::string SettingsFileName() const { return ""; }
};
