#pragma once

#include "filesys.hpp"

#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>

class AssetProcessor;  // defined in asset_processor.hpp

// ---------------------------------------------------------------------------
// ResourceGraph — directed dependency graph of asset-processing nodes.
//
// Each node represents one unit of asset work with optional input/output files,
// a YAML configuration, a build timestamp, a processor type identifier, and
// edges to its dependencies and dependants.
//
// The graph is fully serialisable to YAML in the output-asset directory so that
// incremental builds survive process restarts.  The processorType string is
// stored in the cache and used by ResourceGraph::Build() to look up the
// registered AssetProcessor that should handle the node.
//
// Typical lifecycle:
//   1. Construct ResourceGraph.
//   2. Call LoadCache() to restore previously-saved timestamps.
//   3. Walk the input-asset directory; for each file, call AddNode() the first
//      time it's seen (or locate the existing node via FindByInput()).
//   4. For each AssetProcessor, call its BuildNodes() virtual method to let it
//      add processor-specific intermediate/output nodes and edges.  The
//      processor should skip files whose node timestamp is already up-to-date
//      (node.timestamp >= input file mtime).
//   5. Call Build() to propagate staleness and dispatch stale nodes to the
//      registered AssetProcessor matching each node's processorType.
//   6. Call SaveCache() to persist the updated timestamps.
// ---------------------------------------------------------------------------

using NodeId = std::size_t;
inline constexpr NodeId kInvalidNode = std::numeric_limits<std::size_t>::max();

// ---------------------------------------------------------------------------
// ResourceNode
// ---------------------------------------------------------------------------

struct ResourceNode {
    NodeId id = kInvalidNode;

    // Paths relative to the input-asset root and output-asset root respectively.
    // Both are optional — a node may be purely virtual.
    std::optional<std::filesystem::path> inputFile;
    std::optional<std::filesystem::path> outputFile;

    // Per-node YAML configuration (e.g. texture mip settings, shader flags).
    // May be null/empty.
    YAML::Node config;

    // Timestamp of the last successful build of this node.
    // kUnbuiltTimestamp (the epoch) means "never built" and is always stale.
    // Serialised to/from YAML as an ISO 8601 string with nanosecond precision.
    Timestamp timestamp = kUnbuiltTimestamp;

    // Identifies which AssetProcessor is responsible for building this node.
    // Matched against the string returned by AssetProcessor::TypeName().
    // Empty means no processor is needed (source-file marker nodes).
    std::string processorType;

    // Incoming edges: nodes that must be up-to-date before this node can be
    // (re)built.
    std::vector<NodeId> dependencies;

    // Outgoing edges: nodes that consume this node's output and must be
    // considered for rebuild whenever this node's timestamp advances.
    std::vector<NodeId> dependants;
};

// ---------------------------------------------------------------------------
// ResourceGraph
// ---------------------------------------------------------------------------

class ResourceGraph {
public:
    // Name of the YAML cache file written inside the output-asset directory.
    static constexpr const char* kCacheFileName = ".asset_graph.yaml";

    // Both roots must be absolute directories that exist (or will exist) on
    // disk.  inputRoot is where source assets live; outputRoot is where
    // processed assets are written and where the YAML cache file is stored.
    // An optional FileSystem implementation may be injected for testing;
    // production code uses RealFileSystem::Instance() by default.
    explicit ResourceGraph(std::filesystem::path inputRoot,
                           std::filesystem::path outputRoot,
                           FileSystem& fs = RealFileSystem::Instance());

    // -----------------------------------------------------------------------
    // Graph construction
    // -----------------------------------------------------------------------

    // Append a new node and return its assigned NodeId.
    // The node's 'id' field is set automatically.
    NodeId AddNode(ResourceNode node);

    // Access a node by id.  Behaviour is undefined for an invalid id.
    ResourceNode&       GetNode(NodeId id);
    const ResourceNode& GetNode(NodeId id) const;

    // Find a node by relative input/output path.
    // Returns kInvalidNode when no match is found.
    NodeId FindByInput (const std::filesystem::path& relPath) const;
    NodeId FindByOutput(const std::filesystem::path& relPath) const;

    // Add a directed dependency edge: 'dependency' must be built before
    // 'dependent'.  The reverse (dependant) edge is inserted automatically.
    // Both ids must already exist in the graph.
    void AddEdge(NodeId dependency, NodeId dependent);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    std::size_t                      NodeCount() const;
    std::vector<ResourceNode>&       Nodes();
    const std::vector<ResourceNode>& Nodes() const;

    // -----------------------------------------------------------------------
    // Persistence
    // -----------------------------------------------------------------------

    // Load node timestamps, configs, and processorType strings from the YAML
    // cache file in the output-asset root.  Only nodes whose relative
    // inputFile or outputFile path matches an existing node in the graph are
    // updated; unknown entries are ignored.
    // Returns true if the file existed and was parsed without error.
    bool LoadCache();

    // Write all node timestamps, configs, and processorType strings to the
    // YAML cache file in the output-asset root.  Creates parent directories
    // as needed.
    void SaveCache() const;

    // Accessors for the root directories supplied at construction.
    const std::filesystem::path& InputRoot()  const;
    const std::filesystem::path& OutputRoot() const;

    // Convenience: resolve a node's relative inputFile to an absolute path.
    // Returns an empty path if inputFile is not set.
    std::filesystem::path AbsoluteInputPath (const ResourceNode& node) const;

    // Convenience: resolve a node's relative outputFile to an absolute path.
    // Returns an empty path if outputFile is not set.
    std::filesystem::path AbsoluteOutputPath(const ResourceNode& node) const;

    // Convenience: walk the direct dependencies of an output node to find the
    // first source-file node (inputFile set, empty processorType) and return
    // its absolute input path.  Returns an empty path if none is found.
    std::filesystem::path AbsoluteSourceInputPath(const ResourceNode& node) const;

    // -----------------------------------------------------------------------
    // Config resolution
    // -----------------------------------------------------------------------

    // Collect and merge the configs of all transitive dependency nodes of
    // 'id' that carry a non-empty config field.  Configs are merged in
    // ascending precedence:
    //   1. File-backed dependency nodes (those with an inputFile set), ordered
    //      so that nodes with a shallower inputFile path (fewer path components)
    //      are applied first — i.e. a subdirectory config overrides a root-
    //      level config.
    //   2. Virtual dependency nodes (no inputFile) — applied last with the
    //      highest precedence, regardless of how or where they were added.
    // The target node's own config field is not included; callers that need
    // it layered on top should merge it themselves after this call.
    // Returns an empty YAML::Node if no dependency carries a config.
    YAML::Node ResolveConfig(NodeId id) const;

    // -----------------------------------------------------------------------
    // Processor registry
    // -----------------------------------------------------------------------

    // Register an AssetProcessor under its TypeName().  Build() dispatches
    // stale nodes to the processor whose TypeName() matches the node's
    // processorType.  The graph does not take ownership; the caller must ensure
    // the processor outlives any call to Build().
    void RegisterProcessor(AssetProcessor* processor);

    // -----------------------------------------------------------------------
    // Config invalidation
    // -----------------------------------------------------------------------

    // For each output node (processorType != ""), compare the resolved config
    // computed from the current graph against the config snapshot stored in
    // node.config (written at the end of the previous Build() by RebuildNode).
    // If they differ — because a config file was added, removed, or changed —
    // reset node.timestamp to kUnbuiltTimestamp so Build() treats it as stale.
    // Must be called after LoadCache() and after all graph nodes/edges have
    // been constructed (Passes 1-4), but before Build().
    void InvalidateChangedConfigs();

    // -----------------------------------------------------------------------
    // Build
    // -----------------------------------------------------------------------

    // Propagate staleness through the graph and dispatch stale nodes to their
    // registered AssetProcessor.
    //
    // Algorithm:
    //   - A node is stale when any of its dependencies has a timestamp newer
    //     than the node's own timestamp (or when the node has never been built).
    //   - For every source node (a node with an inputFile whose on-disk mtime
    //     [resolved via inputRoot] is newer than its timestamp), the timestamp
    //     is advanced to match the file's mtime so staleness propagates.
    //   - Iterate until no stale nodes remain:
    //       for each node N across the graph
    //           for each dependant D of N
    //               if N.timestamp > D.timestamp
    //                   look up processor by D.processorType, call Process(),
    //                   then set D.timestamp = now()
    //
    // Nodes are rebuilt in topological order: a dependant is never rebuilt
    // before all of its dependencies are up-to-date.
    //
    // 'verbose' enables one line of output per rebuilt node.
    void Build(bool verbose = false);

    // Returns the last-write-time of an on-disk file as a Timestamp using the
    // real filesystem.  Useful for external callers (e.g. asset_builder.cpp)
    // that always operate on actual disk files.
    // The file must exist; throws std::filesystem::filesystem_error otherwise.
    static Timestamp FileMtime(const std::filesystem::path& absPath);

private:
    std::filesystem::path m_inputRoot;
    std::filesystem::path m_outputRoot;
    FileSystem&           m_fs;

    std::vector<ResourceNode> m_nodes;

    // Fast lookup indices (map relative path string → NodeId).
    std::unordered_map<std::string, NodeId> m_inputIndex;
    std::unordered_map<std::string, NodeId> m_outputIndex;

    // Processor registry: TypeName() → AssetProcessor* (non-owning).
    std::unordered_map<std::string, AssetProcessor*> m_processors;

    // Rebuild node 'id': dispatch to its registered processor then update its timestamp.
    void RebuildNode(NodeId id, bool verbose);
};
