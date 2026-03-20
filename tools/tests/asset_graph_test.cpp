// asset_graph_test.cpp — unit tests for ResourceGraph / FileSystem pipeline.
//
// All tests use a MockFileSystem so no real disk I/O occurs.
// Dummy processors (CopyProcessor, ConfigAwareProcessor) stand in for real
// texture/shader/geometry processors.

#include "asset_graph.hpp"
#include "asset_processor.hpp"
#include "filesys.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers — fixed fake root paths (no real disk access needed)
// ---------------------------------------------------------------------------

// Use generic paths that resolve identically on Windows and Unix when only
// compared via generic_string() in MockFileSystem.
static const fs::path kInput  = fs::path("/") / "asset_test" / "in";
static const fs::path kOutput = fs::path("/") / "asset_test" / "out";

// Helper: add a fixed number of nanoseconds to kUnbuiltTimestamp.
static Timestamp MakeTimestamp(long long ns) {
    return Timestamp(std::chrono::nanoseconds(ns));
}

// ---------------------------------------------------------------------------
// MockFileSystem
// ---------------------------------------------------------------------------

class MockFileSystem : public FileSystem {
    mutable long long m_ns = 0;  // monotonic nanosecond counter

public:
    struct Entry {
        std::string content;
        Timestamp   mtime;
    };

    std::unordered_map<std::string, Entry> files;

    // Advance the internal clock by 'step' nanoseconds and return the new time.
    // Use this to stamp source-file mtimes in tests so they form a clear order.
    Timestamp Tick(long long step = 1'000'000'000LL) const {
        m_ns += step;
        return Timestamp(std::chrono::nanoseconds(m_ns));
    }

    // Add or update a file in the virtual filesystem.
    void Set(const fs::path& path,
             std::string    content = {},
             Timestamp      mtime   = {}) {
        files[path.generic_string()] = {std::move(content), mtime};
    }

    // Update the mtime of an already-registered file.
    void Touch(const fs::path& path, Timestamp mtime) {
        files[path.generic_string()].mtime = mtime;
    }

    // FileSystem interface ---------------------------------------------------

    bool Exists(const fs::path& path) const override {
        return files.count(path.generic_string()) > 0;
    }

    Timestamp LastWriteTime(const fs::path& path) const override {
        auto it = files.find(path.generic_string());
        if (it == files.end())
            throw std::runtime_error("MockFileSystem: file not found: " +
                                     path.generic_string());
        return it->second.mtime;
    }

    void CreateDirectories(const fs::path&) override {} // no-op in memory

    void WriteText(const fs::path& path, std::string_view text) override {
        files[path.generic_string()].content = std::string(text);
    }

    std::string ReadText(const fs::path& path) const override {
        auto it = files.find(path.generic_string());
        if (it == files.end())
            throw std::runtime_error("MockFileSystem: file not found: " +
                                     path.generic_string());
        return it->second.content;
    }

    // Always return a strictly-increasing timestamp so that output nodes built
    // during one Build() call always look newer than any earlier source mtime.
    Timestamp Now() const override { return Tick(); }
};

// ---------------------------------------------------------------------------
// Dummy processors
// ---------------------------------------------------------------------------

// CopyProcessor — produces one output node per input, records every Process call.
class CopyProcessor : public AssetProcessor {
public:
    std::vector<std::string> processedOutputs;  // relative output paths

    std::string TypeName() const override { return "copy"; }

    bool CanProcess(const fs::path& p) const override {
        return p.extension() == ".txt";
    }

    void BuildNodes(ResourceGraph& graph,
                    NodeId          inputNodeId,
                    const fs::path& inputRelPath) override {
        ResourceNode out;
        out.outputFile    = fs::path("out") / inputRelPath;
        out.processorType = TypeName();
        NodeId outId = graph.AddNode(std::move(out));
        graph.AddEdge(inputNodeId, outId);
    }

    void Process(ResourceGraph& /*graph*/, ResourceNode& node) override {
        if (node.outputFile)
            processedOutputs.push_back(node.outputFile->generic_string());
    }
};

// ConfigAwareProcessor — like CopyProcessor but also stores the resolved
// config it received during Process so tests can inspect it.
class ConfigAwareProcessor : public AssetProcessor {
public:
    YAML::Node lastConfig;
    int        processCount = 0;

    std::string TypeName() const override { return "config_aware"; }

    bool CanProcess(const fs::path& p) const override {
        return p.extension() == ".dat";
    }

    void BuildNodes(ResourceGraph& graph,
                    NodeId          inputNodeId,
                    const fs::path& inputRelPath) override {
        ResourceNode out;
        out.outputFile    = fs::path("out") / inputRelPath;
        out.processorType = TypeName();
        NodeId outId = graph.AddNode(std::move(out));
        graph.AddEdge(inputNodeId, outId);
    }

    void Process(ResourceGraph& graph, ResourceNode& node) override {
        lastConfig = graph.ResolveConfig(node.id);
        ++processCount;
    }
};

// ---------------------------------------------------------------------------
// Fixture: GraphTest
// Creates a MockFileSystem and ResourceGraph already populated with a
// registered CopyProcessor.
// ---------------------------------------------------------------------------

class GraphTest : public ::testing::Test {
protected:
    MockFileSystem mock;
    CopyProcessor  copyProc;

    std::unique_ptr<ResourceGraph> graph;

    void SetUp() override {
        graph = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
        graph->RegisterProcessor(&copyProc);
    }

    // Add a source file to the mock FS (absolute path) and insert a
    // source-file marker node into the graph (relative path).
    // The mtime is obtained from mock.Tick() so it is always increasing
    // and ordered before any Now() call that will happen during Build().
    NodeId AddSourceFile(const fs::path& relPath) {
        Timestamp mtime = mock.Tick();
        mock.Set(kInput / relPath, "data", mtime);
        ResourceNode src;
        src.inputFile = relPath;
        return graph->AddNode(std::move(src));
    }

    // Call CopyProcessor::BuildNodes for the given source node.
    void BuildNodesFor(NodeId srcId) {
        const ResourceNode& n = graph->GetNode(srcId);
        copyProc.BuildNodes(*graph, srcId, *n.inputFile);
    }
};

// ---------------------------------------------------------------------------
// Tests — basic build mechanics
// ---------------------------------------------------------------------------

TEST_F(GraphTest, BuildCallsProcessorForStaleNode) {
    NodeId srcId = AddSourceFile("file.txt");
    BuildNodesFor(srcId);

    graph->Build(/*verbose=*/false);

    ASSERT_EQ(copyProc.processedOutputs.size(), 1u);
    EXPECT_EQ(copyProc.processedOutputs.front(), "out/file.txt");
}

TEST_F(GraphTest, BuildDoesNotRebuildUpToDateNode) {
    NodeId srcId = AddSourceFile("file.txt");
    BuildNodesFor(srcId);

    graph->Build(false);  // first build — processor runs
    ASSERT_EQ(copyProc.processedOutputs.size(), 1u);

    graph->Build(false);  // second build — output is already up-to-date
    EXPECT_EQ(copyProc.processedOutputs.size(), 1u);  // still 1
}

TEST_F(GraphTest, BuildRebuildsWhenInputIsNewer) {
    NodeId srcId = AddSourceFile("file.txt");
    BuildNodesFor(srcId);

    graph->Build(false);  // first build
    ASSERT_EQ(copyProc.processedOutputs.size(), 1u);

    // Advance the source file's mtime past whatever Now() returned during
    // the first build.  Tick() guarantees a strictly-later timestamp.
    mock.Touch(kInput / "file.txt", mock.Tick());

    graph->Build(false);  // second build — source is newer → rebuild
    EXPECT_EQ(copyProc.processedOutputs.size(), 2u);
}

TEST_F(GraphTest, OnlyStaleNodeIsRebuilt) {
    NodeId src1 = AddSourceFile("a.txt");
    NodeId src2 = AddSourceFile("b.txt");
    copyProc.BuildNodes(*graph, src1, *graph->GetNode(src1).inputFile);
    copyProc.BuildNodes(*graph, src2, *graph->GetNode(src2).inputFile);

    graph->Build(false);
    ASSERT_EQ(copyProc.processedOutputs.size(), 2u);
    copyProc.processedOutputs.clear();

    // Only touch the second source (strictly later than any Now() so far).
    mock.Touch(kInput / "b.txt", mock.Tick());

    graph->Build(false);
    ASSERT_EQ(copyProc.processedOutputs.size(), 1u);
    EXPECT_EQ(copyProc.processedOutputs.front(), "out/b.txt");
}

TEST_F(GraphTest, BuildWithNoNodesToProcessDoesNotThrow) {
    EXPECT_NO_THROW(graph->Build(false));
}

// ---------------------------------------------------------------------------
// Tests — config resolution
// ---------------------------------------------------------------------------

class ConfigTest : public ::testing::Test {
protected:
    MockFileSystem      mock;
    ConfigAwareProcessor proc;

    std::unique_ptr<ResourceGraph> graph;

    void SetUp() override {
        graph = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
        graph->RegisterProcessor(&proc);
    }

    NodeId AddSourceFile(const fs::path& relPath) {
        Timestamp mtime = mock.Tick();
        mock.Set(kInput / relPath, "data", mtime);
        ResourceNode src;
        src.inputFile = relPath;
        return graph->AddNode(std::move(src));
    }

    // Add a file-backed config node (depth = path component count).
    NodeId AddFileDirConfig(const fs::path& configRelPath, YAML::Node cfg) {
        ResourceNode cfgNode;
        cfgNode.inputFile = configRelPath;
        cfgNode.config    = std::move(cfg);
        return graph->AddNode(std::move(cfgNode));
    }

    // Add a virtual config node (no inputFile — highest precedence).
    NodeId AddVirtualConfig(YAML::Node cfg) {
        ResourceNode cfgNode;
        cfgNode.config = std::move(cfg);
        return graph->AddNode(std::move(cfgNode));
    }
};

TEST_F(ConfigTest, ResolveConfig_ReturnsEmptyWhenNoDeps) {
    NodeId srcId = AddSourceFile("file.dat");
    YAML::Node cfg = graph->ResolveConfig(srcId);
    EXPECT_FALSE(cfg.IsMap() && cfg.size() > 0);
}

TEST_F(ConfigTest, ResolveConfig_ShallowestFileDirFirst) {
    // Root config sets key=root; subdir config sets key=sub.
    // The subdir config should win (deeper = higher precedence).
    YAML::Node rootCfg;  rootCfg["mode"] = "root";
    YAML::Node subCfg;   subCfg ["mode"] = "sub";

    NodeId srcId   = AddSourceFile("subdir/file.dat");
    NodeId rootId  = AddFileDirConfig("root.yaml",         rootCfg);  // depth 1
    NodeId subId   = AddFileDirConfig("subdir/sub.yaml",   subCfg);   // depth 2

    graph->AddEdge(rootId, srcId);
    graph->AddEdge(subId,  srcId);

    YAML::Node resolved = graph->ResolveConfig(srcId);
    EXPECT_EQ(resolved["mode"].as<std::string>(), "sub");
}

TEST_F(ConfigTest, ResolveConfig_VirtualOverridesFileBacked) {
    YAML::Node fileCfg;    fileCfg   ["mode"] = "file";
    YAML::Node virtualCfg; virtualCfg["mode"] = "virtual";

    NodeId srcId    = AddSourceFile("file.dat");
    NodeId fileId   = AddFileDirConfig("root.yaml", fileCfg);
    NodeId virtualId = AddVirtualConfig(virtualCfg);

    graph->AddEdge(fileId,    srcId);
    graph->AddEdge(virtualId, srcId);

    YAML::Node resolved = graph->ResolveConfig(srcId);
    EXPECT_EQ(resolved["mode"].as<std::string>(), "virtual");
}

TEST_F(ConfigTest, ResolveConfig_MergesDistinctKeys) {
    YAML::Node cfgA; cfgA["width"]  = 256;
    YAML::Node cfgB; cfgB["height"] = 512;

    NodeId srcId = AddSourceFile("file.dat");
    NodeId aId   = AddFileDirConfig("a.yaml", cfgA);  // depth 1
    NodeId bId   = AddFileDirConfig("sub/b.yaml", cfgB); // depth 2

    graph->AddEdge(aId, srcId);
    graph->AddEdge(bId, srcId);

    YAML::Node resolved = graph->ResolveConfig(srcId);
    EXPECT_EQ(resolved["width"].as<int>(),  256);
    EXPECT_EQ(resolved["height"].as<int>(), 512);
}

TEST_F(ConfigTest, ProcessReceivesResolvedConfig) {
    YAML::Node cfg; cfg["quality"] = 8;

    NodeId srcId = AddSourceFile("file.dat");
    NodeId cfgId = AddFileDirConfig("settings.yaml", cfg);
    graph->AddEdge(cfgId, srcId);

    // Build the output node manually.
    proc.BuildNodes(*graph, srcId, *graph->GetNode(srcId).inputFile);
    graph->Build(false);

    ASSERT_EQ(proc.processCount, 1);
    EXPECT_EQ(proc.lastConfig["quality"].as<int>(), 8);
}

// ---------------------------------------------------------------------------
// Tests — timestamp-based invalidation
//
// Invalidation is purely timestamp-driven: Build() propagates timestamps
// upward through all node types (Phase 1.5) so any upstream change makes
// downstream output nodes stale without requiring config-string comparison.
// ---------------------------------------------------------------------------

class InvalidationTest : public ::testing::Test {
protected:
    MockFileSystem      mock;
    ConfigAwareProcessor proc;

    std::unique_ptr<ResourceGraph> graph;

    NodeId srcId  = kInvalidNode;
    NodeId outId  = kInvalidNode;

    void SetUp() override {
        graph = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
        graph->RegisterProcessor(&proc);

        // Source file
        mock.Set(kInput / "file.dat", "data", mock.Tick());
        ResourceNode src;
        src.inputFile = fs::path("file.dat");
        srcId = graph->AddNode(std::move(src));

        // Output node
        proc.BuildNodes(*graph, srcId, *graph->GetNode(srcId).inputFile);
        outId = graph->GetNode(srcId).dependants.front();
    }
};

// After a build with no upstream changes, a second Build() must not rebuild.
TEST_F(InvalidationTest, NoChange_OutputNotRebuilt) {
    graph->Build(false);
    ASSERT_EQ(proc.processCount, 1);

    graph->Build(false);
    EXPECT_EQ(proc.processCount, 1);  // no second rebuild
}

// A virtual config node whose upstream dep timestamp advances should cause
// the downstream output node to be rebuilt on the next Build() call.
// This simulates a GLB file being modified so that a texture's linear_mips
// classification changes — no file YAML change, purely a timestamp signal.
TEST_F(InvalidationTest, UpdatedVirtualConfigDep_TriggersRebuild) {
    // Add a virtual config node (no inputFile) that is a dep of the source.
    YAML::Node cfg; cfg["linear_mips"] = false;
    ResourceNode cfgNode;
    cfgNode.config    = cfg;
    cfgNode.timestamp = mock.Tick();  // initial timestamp
    NodeId cfgId = graph->AddNode(std::move(cfgNode));
    graph->AddEdge(cfgId, srcId);

    graph->Build(false);
    ASSERT_EQ(proc.processCount, 1);

    // Advance the virtual config node's timestamp (simulating its upstream
    // GLB source being modified and reclassifying the texture).
    graph->GetNode(cfgId).timestamp = mock.Tick();

    graph->Build(false);
    EXPECT_EQ(proc.processCount, 2);  // rebuilt because config dep is newer
}

// ---------------------------------------------------------------------------
// Tests — cache persistence
// ---------------------------------------------------------------------------

class CacheTest : public ::testing::Test {
protected:
    MockFileSystem mock;
    CopyProcessor  copyProc;
};

TEST_F(CacheTest, SaveAndLoadRestoresTimestamp) {
    // Build in graph #1
    auto g1 = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
    g1->RegisterProcessor(&copyProc);

    mock.Set(kInput / "file.txt", "data", MakeTimestamp(1'000'000'000LL));
    ResourceNode src1; src1.inputFile = fs::path("file.txt");
    NodeId srcId1 = g1->AddNode(std::move(src1));
    copyProc.BuildNodes(*g1, srcId1, *g1->GetNode(srcId1).inputFile);
    g1->Build(false);

    NodeId outId1 = g1->GetNode(srcId1).dependants.front();
    Timestamp builtTs = g1->GetNode(outId1).timestamp;
    ASSERT_GT(builtTs, kUnbuiltTimestamp);

    g1->SaveCache();

    // Load into graph #2 (same mock, so the cache YAML is accessible)
    CopyProcessor copyProc2;
    auto g2 = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
    g2->RegisterProcessor(&copyProc2);

    mock.Set(kInput / "file.txt", "data", MakeTimestamp(1'000'000'000LL));
    ResourceNode src2; src2.inputFile = fs::path("file.txt");
    NodeId srcId2 = g2->AddNode(std::move(src2));
    copyProc2.BuildNodes(*g2, srcId2, *g2->GetNode(srcId2).inputFile);

    bool loaded = g2->LoadCache();
    EXPECT_TRUE(loaded);

    NodeId outId2 = g2->GetNode(srcId2).dependants.front();
    EXPECT_EQ(g2->GetNode(outId2).timestamp, builtTs);
}

TEST_F(CacheTest, LoadCache_ReturnsFalseWhenNoCacheFile) {
    auto g = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
    EXPECT_FALSE(g->LoadCache());
}

TEST_F(CacheTest, CachedTimestampPreventsRebuild) {
    // Build and save.
    auto g1 = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
    g1->RegisterProcessor(&copyProc);

    mock.Set(kInput / "file.txt", "data", MakeTimestamp(1'000'000'000LL));
    ResourceNode src1; src1.inputFile = fs::path("file.txt");
    NodeId srcId1 = g1->AddNode(std::move(src1));
    copyProc.BuildNodes(*g1, srcId1, *g1->GetNode(srcId1).inputFile);
    g1->Build(false);
    g1->SaveCache();

    // Create a fresh graph, load the cache — should not trigger a rebuild.
    CopyProcessor freshProc;
    auto g2 = std::make_unique<ResourceGraph>(kInput, kOutput, mock);
    g2->RegisterProcessor(&freshProc);

    mock.Set(kInput / "file.txt", "data", MakeTimestamp(1'000'000'000LL));
    ResourceNode src2; src2.inputFile = fs::path("file.txt");
    NodeId srcId2 = g2->AddNode(std::move(src2));
    freshProc.BuildNodes(*g2, srcId2, *g2->GetNode(srcId2).inputFile);
    g2->LoadCache();
    g2->Build(false);

    EXPECT_TRUE(freshProc.processedOutputs.empty());
}

// ---------------------------------------------------------------------------
// Tests — graph structure and edge semantics
// ---------------------------------------------------------------------------

TEST(GraphStructureTest, AddEdge_WiresBothDirections) {
    MockFileSystem mock;
    ResourceGraph  g(kInput, kOutput, mock);

    ResourceNode a; a.inputFile  = fs::path("a.txt");
    ResourceNode b; b.outputFile = fs::path("b.txt"); b.processorType = "copy";
    NodeId aId = g.AddNode(std::move(a));
    NodeId bId = g.AddNode(std::move(b));
    g.AddEdge(aId, bId);

    EXPECT_EQ(g.GetNode(bId).dependencies.front(), aId);
    EXPECT_EQ(g.GetNode(aId).dependants.front(),   bId);
}

TEST(GraphStructureTest, AddEdge_NoDuplicates) {
    MockFileSystem mock;
    ResourceGraph  g(kInput, kOutput, mock);

    ResourceNode a; a.inputFile  = fs::path("a.txt");
    ResourceNode b; b.outputFile = fs::path("b.txt"); b.processorType = "copy";
    NodeId aId = g.AddNode(std::move(a));
    NodeId bId = g.AddNode(std::move(b));
    g.AddEdge(aId, bId);
    g.AddEdge(aId, bId);  // duplicate

    EXPECT_EQ(g.GetNode(bId).dependencies.size(), 1u);
    EXPECT_EQ(g.GetNode(aId).dependants.size(),   1u);
}

TEST(GraphStructureTest, FindByInput_ReturnsCorrectId) {
    MockFileSystem mock;
    ResourceGraph  g(kInput, kOutput, mock);

    ResourceNode n; n.inputFile = fs::path("textures/banner.png");
    NodeId id = g.AddNode(std::move(n));

    EXPECT_EQ(g.FindByInput(fs::path("textures/banner.png")), id);
    EXPECT_EQ(g.FindByInput(fs::path("other.png")), kInvalidNode);
}

TEST(GraphStructureTest, FindByOutput_ReturnsCorrectId) {
    MockFileSystem mock;
    ResourceGraph  g(kInput, kOutput, mock);

    ResourceNode n; n.outputFile = fs::path("textures/banner.ktx2");
    NodeId id = g.AddNode(std::move(n));

    EXPECT_EQ(g.FindByOutput(fs::path("textures/banner.ktx2")), id);
    EXPECT_EQ(g.FindByOutput(fs::path("other.ktx2")), kInvalidNode);
}

TEST(GraphStructureTest, AbsolutePaths_CombineRootsCorrectly) {
    MockFileSystem mock;
    ResourceGraph  g(kInput, kOutput, mock);

    ResourceNode n;
    n.inputFile  = fs::path("sub/file.txt");
    n.outputFile = fs::path("sub/file.out");
    NodeId id = g.AddNode(std::move(n));

    EXPECT_EQ(g.AbsoluteInputPath (g.GetNode(id)), kInput  / "sub" / "file.txt");
    EXPECT_EQ(g.AbsoluteOutputPath(g.GetNode(id)), kOutput / "sub" / "file.out");
}
