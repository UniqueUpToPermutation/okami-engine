#include "asset_graph.hpp"
#include "asset_processor.hpp"
#include "filesys.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Timestamp helpers
// ---------------------------------------------------------------------------

// Format: "2026-03-19T14:05:32.001234567Z"
static std::string TimestampToString(Timestamp ts) {
    using namespace std::chrono;

    auto secs    = time_point_cast<seconds>(ts);
    auto ns_part = duration_cast<nanoseconds>(ts - secs).count();
    std::time_t tt = system_clock::to_time_t(
        time_point_cast<system_clock::duration>(secs));

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif

    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setw(9) << std::setfill('0') << ns_part << 'Z';
    return oss.str();
}

static Timestamp StringToTimestamp(const std::string& s) {
    using namespace std::chrono;

    if (s.empty()) return kUnbuiltTimestamp;

    std::tm utc{};
    std::istringstream iss(s);
    iss >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) return kUnbuiltTimestamp;
#if defined(_WIN32)
    std::time_t tt = _mkgmtime(&utc);
#else
    std::time_t tt = timegm(&utc);
#endif
    if (tt == (std::time_t)-1) return kUnbuiltTimestamp;

    auto base = time_point_cast<nanoseconds>(system_clock::from_time_t(tt));

    // Parse optional ".NNNNNNNNN" sub-second part.
    char ch = '\0';
    if (iss.get(ch) && ch == '.') {
        std::string frac;
        while (iss.peek() != 'Z' && iss.peek() != EOF)
            frac += (char)iss.get();
        // Pad or truncate to 9 digits.
        frac.resize(9, '0');
        long long ns = std::stoll(frac);
        base += nanoseconds(ns);
    }

    return base;
}

// ---------------------------------------------------------------------------
// ResourceGraph — construction
// ---------------------------------------------------------------------------

ResourceGraph::ResourceGraph(fs::path inputRoot, fs::path outputRoot, FileSystem& fsys)
    : m_inputRoot(std::move(inputRoot))
    , m_outputRoot(std::move(outputRoot))
    , m_fs(fsys) {}

NodeId ResourceGraph::AddNode(ResourceNode node) {
    NodeId id = m_nodes.size();
    node.id   = id;

    if (node.inputFile)
        m_inputIndex[node.inputFile->generic_string()] = id;
    if (node.outputFile)
        m_outputIndex[node.outputFile->generic_string()] = id;

    m_nodes.push_back(std::move(node));
    return id;
}

ResourceNode& ResourceGraph::GetNode(NodeId id) {
    assert(id < m_nodes.size());
    return m_nodes[id];
}

const ResourceNode& ResourceGraph::GetNode(NodeId id) const {
    assert(id < m_nodes.size());
    return m_nodes[id];
}

NodeId ResourceGraph::FindByInput(const fs::path& relPath) const {
    auto it = m_inputIndex.find(relPath.generic_string());
    return it != m_inputIndex.end() ? it->second : kInvalidNode;
}

NodeId ResourceGraph::FindByOutput(const fs::path& relPath) const {
    auto it = m_outputIndex.find(relPath.generic_string());
    return it != m_outputIndex.end() ? it->second : kInvalidNode;
}

void ResourceGraph::AddEdge(NodeId dependency, NodeId dependent) {
    assert(dependency < m_nodes.size());
    assert(dependent  < m_nodes.size());

    auto& dep  = m_nodes[dependency];
    auto& pend = m_nodes[dependent];

    // Avoid duplicates.
    if (std::find(pend.dependencies.begin(), pend.dependencies.end(), dependency)
            == pend.dependencies.end()) {
        pend.dependencies.push_back(dependency);
        dep.dependants.push_back(dependent);
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

std::size_t ResourceGraph::NodeCount() const { return m_nodes.size(); }

std::vector<ResourceNode>& ResourceGraph::Nodes() { return m_nodes; }
const std::vector<ResourceNode>& ResourceGraph::Nodes() const { return m_nodes; }

const fs::path& ResourceGraph::InputRoot()  const { return m_inputRoot;  }
const fs::path& ResourceGraph::OutputRoot() const { return m_outputRoot; }

fs::path ResourceGraph::AbsoluteInputPath(const ResourceNode& node) const {
    return node.inputFile ? (m_inputRoot / *node.inputFile) : fs::path{};
}

fs::path ResourceGraph::AbsoluteOutputPath(const ResourceNode& node) const {
    return node.outputFile ? (m_outputRoot / *node.outputFile) : fs::path{};
}

fs::path ResourceGraph::AbsoluteSourceInputPath(const ResourceNode& node) const {
    for (NodeId depId : node.dependencies) {
        const ResourceNode& dep = m_nodes[depId];
        if (dep.inputFile && dep.processorType.empty())
            return m_inputRoot / *dep.inputFile;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

bool ResourceGraph::LoadCache() {
    fs::path cacheFile = m_outputRoot / kCacheFileName;
    if (!m_fs.Exists(cacheFile)) return false;

    YAML::Node root;
    try {
        root = YAML::Load(m_fs.ReadText(cacheFile));
    } catch (const std::exception& e) {
        std::cerr << "Warning: could not load asset graph cache "
                  << cacheFile << ": " << e.what() << "\n";
        return false;
    }

    if (!root || !root.IsSequence()) return false;

    for (const auto& entry : root) {
        // Locate the matching node by outputFile first (unique across all nodes)
        // then fall back to inputFile for source/config nodes that have no output.
        NodeId id = kInvalidNode;

        if (entry["outputFile"]) {
            id = FindByOutput(fs::path(entry["outputFile"].as<std::string>()));
        }
        if (id == kInvalidNode && entry["inputFile"]) {
            id = FindByInput(fs::path(entry["inputFile"].as<std::string>()));
        }
        if (id == kInvalidNode) continue;

        ResourceNode& node = m_nodes[id];

        if (entry["timestamp"])
            node.timestamp = StringToTimestamp(entry["timestamp"].as<std::string>());
        if (entry["processorType"])
            node.processorType = entry["processorType"].as<std::string>();
        // Note: "config" is saved to the cache for debugging/inspection but is
        // never loaded back — invalidation is driven purely by timestamps.

        // Restore cached dependency edges (topology).  Edges that come from
        // virtual nodes (no path) are not cached — BuildNodes recreates them.
        // AddEdge deduplicates, so calling it after the passes is safe.
        if (entry["deps"] && entry["deps"].IsSequence()) {
            for (const auto& dep : entry["deps"]) {
                NodeId depId = kInvalidNode;
                if (dep["outputFile"])
                    depId = FindByOutput(fs::path(dep["outputFile"].as<std::string>()));
                if (depId == kInvalidNode && dep["inputFile"])
                    depId = FindByInput(fs::path(dep["inputFile"].as<std::string>()));
                if (depId != kInvalidNode && depId != id)
                    AddEdge(depId, id);
            }
        }
    }

    return true;
}

void ResourceGraph::SaveCache() const {
    fs::path cacheFile = m_outputRoot / kCacheFileName;
    m_fs.CreateDirectories(cacheFile.parent_path());

    YAML::Emitter out;
    out << YAML::BeginSeq;

    for (const auto& node : m_nodes) {
        out << YAML::Flow << YAML::BeginMap;

        if (node.inputFile)
            out << YAML::Key << "inputFile"
                << YAML::Value << node.inputFile->generic_string();
        if (node.outputFile)
            out << YAML::Key << "outputFile"
                << YAML::Value << node.outputFile->generic_string();

        out << YAML::Key << "timestamp"
            << YAML::Value << TimestampToString(node.timestamp);

        if (!node.processorType.empty())
            out << YAML::Key << "processorType"
                << YAML::Value << node.processorType;

        if (node.config && node.config.IsMap() && node.config.size() > 0)
            out << YAML::Key << "config" << YAML::Value
                << YAML::Flow << node.config;

        // For output nodes, emit the fully-resolved config so the cache file
        // shows exactly what the processor received (useful for debugging).
        if (!node.processorType.empty()) {
            NodeId id = static_cast<NodeId>(&node - m_nodes.data());
            YAML::Node resolved = ResolveConfig(id);
            if (resolved && resolved.IsMap() && resolved.size() > 0)
                out << YAML::Key << "resolvedConfig" << YAML::Value
                    << YAML::Flow << resolved;
        }

        // Serialize dependency edges (topology) for nodes that have at least
        // one dep with a stable path reference (inputFile or outputFile).
        // Virtual-only deps (no path) are recreated each run by BuildNodes.
        {
            bool hasCacheableDeps = false;
            for (NodeId depId : node.dependencies) {
                const ResourceNode& dep = m_nodes[depId];
                if (dep.inputFile || dep.outputFile) { hasCacheableDeps = true; break; }
            }
            if (hasCacheableDeps) {
                out << YAML::Key << "deps" << YAML::Value << YAML::Flow << YAML::BeginSeq;
                for (NodeId depId : node.dependencies) {
                    const ResourceNode& dep = m_nodes[depId];
                    if (dep.outputFile) {
                        out << YAML::BeginMap
                            << YAML::Key << "outputFile"
                            << YAML::Value << dep.outputFile->generic_string()
                            << YAML::EndMap;
                    } else if (dep.inputFile) {
                        out << YAML::BeginMap
                            << YAML::Key << "inputFile"
                            << YAML::Value << dep.inputFile->generic_string()
                            << YAML::EndMap;
                    }
                    // Virtual nodes (neither path) are skipped — they are
                    // recreated by BuildNodes on every run.
                }
                out << YAML::EndSeq;
            }
        }

        out << YAML::EndMap;
    }

    out << YAML::EndSeq;

    m_fs.WriteText(cacheFile, out.c_str());
}

// ---------------------------------------------------------------------------
// Config resolution
// ---------------------------------------------------------------------------

YAML::Node ResourceGraph::ResolveConfig(NodeId id) const {
    assert(id < m_nodes.size());

    // Collect all transitive dependency nodes that have a non-empty config,
    // using BFS/DFS over the dependency edges.
    std::vector<NodeId> visited;
    {
        std::vector<NodeId> stack = {id};
        while (!stack.empty()) {
            NodeId cur = stack.back(); stack.pop_back();
            for (NodeId dep : m_nodes[cur].dependencies) {
                if (std::find(visited.begin(), visited.end(), dep) == visited.end()) {
                    visited.push_back(dep);
                    stack.push_back(dep);
                }
            }
        }
    }

    // Partition into file-backed and virtual config nodes.
    // File-backed nodes are further split into:
    //   - directory-level settings (inputFile stem has no extension,
    //     e.g. "texture.yaml" → stem "texture")
    //   - per-file overrides (inputFile stem itself has an extension,
    //     e.g. "test2.png.yaml" → stem "test2.png" → ext ".png")
    // Virtual nodes (no inputFile) have the highest precedence of all.
    struct FileBacked { NodeId id; std::size_t depth; bool isPerFile; };
    std::vector<FileBacked> fileBacked;
    std::vector<NodeId>     virtualNodes;

    for (NodeId dep : visited) {
        const ResourceNode& n = m_nodes[dep];
        if (!n.config || !n.config.IsMap() || n.config.size() == 0) continue;

        if (n.inputFile) {
            std::size_t depth = std::distance(n.inputFile->begin(),
                                              n.inputFile->end());
            bool isPerFile = !n.inputFile->stem().extension().empty();
            fileBacked.push_back({dep, depth, isPerFile});
        } else {
            virtualNodes.push_back(dep);
        }
    }

    // Sort: directory settings first (shallow→deep), then per-file overrides
    // (shallow→deep).  Virtual nodes are applied last, after both.
    std::sort(fileBacked.begin(), fileBacked.end(),
              [](const FileBacked& a, const FileBacked& b) {
                  if (a.isPerFile != b.isPerFile) return !a.isPerFile;
                  return a.depth < b.depth;
              });

    // Merge in order: file-backed shallowest first, then virtual.
    YAML::Node merged;
    auto applyNode = [&](NodeId nid) {
        const YAML::Node& cfg = m_nodes[nid].config;
        for (const auto& kv : cfg)
            merged[kv.first.Scalar()] = kv.second;
    };

    for (const auto& fb : fileBacked)
        applyNode(fb.id);
    for (NodeId vn : virtualNodes)
        applyNode(vn);

    return merged;
}

// ---------------------------------------------------------------------------
// Processor registry
// ---------------------------------------------------------------------------

void ResourceGraph::RegisterProcessor(AssetProcessor* processor) {
    assert(processor);
    m_processors[processor->TypeName()] = processor;
}

// ---------------------------------------------------------------------------
// Config invalidation
// ---------------------------------------------------------------------------

void ResourceGraph::InvalidateChangedConfigs() {
    // Invalidation is now purely timestamp-based: Build() propagates timestamps
    // through all node types (including virtual config nodes) so any upstream
    // change automatically makes downstream output nodes stale.  This function
    // is kept as a no-op so callers do not need to be updated.
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

Timestamp ResourceGraph::FileMtime(const fs::path& absPath) {
    return FileTimeToTimestamp(fs::last_write_time(absPath));
}

void ResourceGraph::RebuildNode(NodeId id, bool verbose) {
    ResourceNode& node = m_nodes[id];

    if (!node.processorType.empty()) {
        auto it = m_processors.find(node.processorType);
        if (it == m_processors.end()) {
            throw std::runtime_error(
                "No processor registered for type '" + node.processorType +
                "' (needed by node " + std::to_string(id) + ")");
        }

        // Ensure output directory exists.
        if (node.outputFile) {
            fs::path absOut = AbsoluteOutputPath(node);
            m_fs.CreateDirectories(absOut.parent_path());
        }

            it->second->Process(*this, node);
    }

    node.timestamp = m_fs.Now();

    if (verbose) {
        std::string label = node.outputFile
            ? node.outputFile->generic_string()
            : (node.inputFile ? node.inputFile->generic_string()
                              : "(virtual:" + std::to_string(id) + ")");
        std::cout << "  built  " << label << "\n";
    }
}

void ResourceGraph::Build(bool verbose) {
    // Phase 1: advance timestamps of source-file nodes whose on-disk mtime is
    // newer than the cached timestamp.
    for (auto& node : m_nodes) {
        if (!node.inputFile) continue;
        fs::path abs = m_inputRoot / *node.inputFile;
        if (!m_fs.Exists(abs)) continue;
        Timestamp mtime = m_fs.LastWriteTime(abs);
        if (mtime > node.timestamp)
            node.timestamp = mtime;
    }

    // Phase 1.5: propagate timestamps upward through non-output nodes (config
    // and virtual nodes).  This ensures that a virtual config node whose
    // upstream dependency (e.g. a .glb source file) has a newer timestamp
    // passes that advancement on to its dependants before Phase 2 runs.
    // Repeat until stable (handle arbitrary depth chains).
    {
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& node : m_nodes) {
                if (!node.processorType.empty()) continue;  // output nodes handled in Phase 2
                for (NodeId depId : node.dependencies) {
                    const Timestamp& dts = m_nodes[depId].timestamp;
                    if (dts > node.timestamp) {
                        node.timestamp = dts;
                        changed = true;
                    }
                }
            }
        }
    }

    // Phase 2: topological fixpoint — repeatedly find and rebuild any
    // dependant D whose oldest dependency N has timestamp > D.timestamp.
    // We use Kahn's-style processing: maintain a queue of nodes that are
    // ready (all dependencies up-to-date).

    // Build in-degree table based on staleness — a node is ready when all
    // its dependencies have timestamps >= its own timestamp.
    // We iterate until no work remains.
    bool anyWork = true;
    std::unordered_set<NodeId> rebuilt;
    while (anyWork) {
        anyWork = false;
        for (auto& node : m_nodes) {
            if (node.processorType.empty()) continue;  // no build action needed

            // Find the maximum dependency timestamp.
            Timestamp maxDepTs = kUnbuiltTimestamp;
            for (NodeId depId : node.dependencies) {
                const Timestamp& dts = m_nodes[depId].timestamp;
                if (dts > maxDepTs) maxDepTs = dts;
            }

            if (maxDepTs == kUnbuiltTimestamp && node.dependencies.empty())
                continue;  // no dependencies, nothing to drive a rebuild

            if (maxDepTs > node.timestamp) {
                // All dependencies must themselves be up-to-date first.
                bool depsReady = true;
                for (NodeId depId : node.dependencies) {
                    const ResourceNode& dep = m_nodes[depId];
                    // A dependency is not ready if it still has a stale
                    // processing node of its own (i.e. it needs rebuilding).
                    if (!dep.processorType.empty()) {
                        Timestamp depMaxDep = kUnbuiltTimestamp;
                        for (NodeId d2 : dep.dependencies)
                            if (m_nodes[d2].timestamp > depMaxDep)
                                depMaxDep = m_nodes[d2].timestamp;
                        if (depMaxDep > dep.timestamp) {
                            depsReady = false;
                            break;
                        }
                    }
                }
                if (!depsReady) continue;

                RebuildNode(node.id, verbose);
                rebuilt.insert(node.id);
                anyWork = true;
            }
        }
    }

    if (verbose) {
        for (const auto& node : m_nodes) {
            if (node.processorType.empty()) continue;
            if (rebuilt.count(node.id)) continue;
            std::string label = node.outputFile
                ? node.outputFile->generic_string()
                : (node.inputFile ? node.inputFile->generic_string()
                                  : "(virtual:" + std::to_string(node.id) + ")");
            std::cout << "  skip   " << label << "\n";
        }
    }
}
