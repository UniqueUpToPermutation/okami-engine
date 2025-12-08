#include "../jobs.hpp"
#include <gtest/gtest.h>
#include <set>

using namespace okami;

// Test message types
struct TestMessage {
    int value;
    std::string text;

    bool operator==(const TestMessage& other) const {
        return value == other.value && text == other.text;
    }
};

struct AnotherMessage {
    float data;
};

// Test for CopyableMessage concept
TEST(JobSystemTest, CopyableMessageConcept) {
    // Test that TestMessage satisfies CopyableMessage
    static_assert(CopyableMessage<TestMessage>, "TestMessage should be copyable");
    static_assert(CopyableMessage<AnotherMessage>, "AnotherMessage should be copyable");
    static_assert(!CopyableMessage<std::unique_ptr<int>>, "unique_ptr should not be copyable");
}

// Test MessageBus
TEST(JobSystemTest, MessageBusEnsureLane) {
    MessageBus bus;
    
    // Ensure lane for TestMessage
    bus.EnsurePort(TestMessage{1, "test"});
    
    // Get the lane - note: GetPort is const, but bus is non-const, should work
    auto lane = bus.GetPort<TestMessage>();
    ASSERT_NE(lane, nullptr);
    
    // Ensure another lane
    bus.EnsurePort(AnotherMessage{3.14f});
    auto anotherLane = bus.GetPort<AnotherMessage>();
    ASSERT_NE(anotherLane, nullptr);
}

TEST(JobSystemTest, MessageBusSendAndReceive) {
    MessageBus bus;
    bus.EnsurePort(TestMessage{});
    
    // Send a message
    TestMessage sent{42, "hello"};
    bus.Send(sent);
    
    // Receive messages
    auto lane = bus.GetPort<TestMessage>();
    ASSERT_NE(lane, nullptr);
    
    std::vector<TestMessage> received;
    lane->Handle([&received](const TestMessage& msg) {
        received.push_back(msg);
    });
    
    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], sent);
}

// Test JobGraph
TEST(JobSystemTest, JobGraphAddNode) {
    JobGraph graph;
    
    auto task = [](JobContext&) { return Error{}; };
    std::vector<int> deps = {};
    
    int nodeId = graph.AddNode(task, deps);
    EXPECT_EQ(nodeId, 0);
    
    const auto& nodes = graph.GetNodes();
    ASSERT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0]->m_id, 0);
    EXPECT_EQ(nodes[0]->m_dependencies.size(), 0);
}

TEST(JobSystemTest, JobGraphAddNodeWithDependencies) {
    JobGraph graph;
    
    // Add first node
    auto task1 = [](JobContext&) { return Error{}; };
    int node1 = graph.AddNode(task1, {});
    
    // Add second node depending on first
    auto task2 = [](JobContext&) { return Error{}; };
    std::vector<int> deps = {node1};
    int node2 = graph.AddNode(task2, deps);
    
    const auto& nodes = graph.GetNodes();
    ASSERT_EQ(nodes.size(), 2);
    EXPECT_EQ(nodes[1]->m_dependencies.size(), 1);
    EXPECT_EQ(nodes[1]->m_dependencies[0], nodes[0]);
    EXPECT_EQ(nodes[0]->m_dependents.size(), 1);
    EXPECT_EQ(nodes[0]->m_dependents[0], nodes[1]);
}

TEST(JobSystemTest, JobGraphInvalidDependency) {
    JobGraph graph;
    
    auto task = [](JobContext&) { return Error{}; };
    std::vector<int> deps = {0};
    
    // Try to add node with invalid dependency
    EXPECT_THROW(graph.AddNode(task, deps), std::out_of_range);
}

TEST(JobSystemTest, JobGraphExecution) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;
    
    // Track execution order
    std::vector<int> executionOrder;
    
    // Add nodes
    auto task1 = [&](JobContext&) { 
        executionOrder.push_back(1);
        return Error{}; 
    };
    int node1 = graph.AddNode(task1, {});
    
    auto task2 = [&](JobContext&) { 
        executionOrder.push_back(2);
        return Error{}; 
    };
    std::vector<int> deps2 = {node1};
    int node2 = graph.AddNode(task2, deps2);
    
    auto task3 = [&](JobContext&) { 
        executionOrder.push_back(3);
        return Error{}; 
    };
    std::vector<int> deps3 = {node1};
    int node3 = graph.AddNode(task3, deps3);
    
    auto task4 = [&](JobContext&) { 
        executionOrder.push_back(4);
        return Error{}; 
    };
    std::vector<int> deps4 = {node2, node3};
    int node4 = graph.AddNode(task4, deps4);
    
    // Execute
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());
    
    // Check execution order - node1 first, then 2 and 3 in some order, then 4
    ASSERT_EQ(executionOrder.size(), 4);
    EXPECT_EQ(executionOrder[0], 1);
    EXPECT_EQ(executionOrder[3], 4);
    // 2 and 3 can be in either order
    std::set<int> middle{executionOrder[1], executionOrder[2]};
    EXPECT_EQ(middle, std::set<int>({2, 3}));
}


TEST(JobSystemTest, AddMessageNode) {
    JobGraph graph;
    MessageBus bus;
    
    // Track received messages
    std::vector<TestMessage> received;
    
    // Producer job
    auto producer = [&](JobContext& ctx, PortOut<TestMessage>& out) mutable {
        out.Send(TestMessage{42, "produced"});
        return Error{};
    };
    int producerNode = graph.AddMessageNode(producer, {});
    
    // Consumer job
    auto consumer = [&](JobContext& ctx, PortIn<TestMessage>& in) mutable {
        in.Handle([&](const TestMessage& msg) {
            received.push_back(msg);
        });
        return Error{};
    };
    std::vector<int> deps = {producerNode};
    int consumerNode = graph.AddMessageNode(consumer, deps);
    
    // Execute
    DefaultJobGraphExecutor executor;
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());
    
    // Check message was received
    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0].value, 42);
    EXPECT_EQ(received[0].text, "produced");
}

TEST(JobSystemTest, JobGraphCycleDetection) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;
    
    // Create a cycle: A -> B -> A
    auto taskA = [](JobContext&) { return Error{}; };
    int nodeA = graph.AddNode(taskA, {});
    
    auto taskB = [](JobContext&) { return Error{}; };
    // B depends on A
    std::vector<int> depsB = {nodeA};
    int nodeB = graph.AddNode(taskB, depsB);
    
    // Make A depend on B (manually, since AddNode checks for existing nodes, but we need to hack it or use a new node that closes the loop)
    // Actually, AddNode doesn't allow adding dependencies to future nodes directly.
    // But we can add a third node C that depends on B, and then somehow make A depend on C?
    // The current API AddNode takes dependencies as indices of *already added* nodes.
    // So we can't easily create a cycle with AddNode unless we modify the graph structure directly or if AddNode allows forward references (it takes ints, so maybe?)
    
    // Wait, AddNode takes `std::span<int const> dependencies`. If I pass an index that hasn't been returned yet, it might be invalid or just an index.
    // But `AddNode` implementation looks up the nodes immediately.
    
    // Let's try to create a cycle by hacking the dependencies after creation, since the public API enforces DAG construction order (mostly).
    // Actually, `AddNode` validates that dependencies exist.
    
    // However, if I have A and B. A depends on nothing. B depends on A.
    // If I can add a dependency to A *after* B is created... but the API doesn't expose that.
    
    // Wait, `JobGraphNode` has `m_dependencies` which is `vector<shared_ptr<JobGraphNode>>`.
    // I can get the nodes and modify them if I have access. `GetNodes()` returns `const vector&`.
    // So I can't modify them easily through the public API.
    
    // But wait, `AddNode` takes indices. If I pass an index that is out of range, it throws.
    // So I can only depend on existing nodes.
    // This means `AddNode` enforces a topological ordering during construction!
    // So it's impossible to create a cycle using `AddNode` alone unless I can make a node depend on itself.
    
    // Let's try self-dependency.
    // int nodeA = graph.AddNode(taskA, {0}); // This would throw because node 0 doesn't exist yet when calling AddNode for the first time.
    
    // What if I do:
    // int nodeA = graph.AddNode(taskA, {});
    // int nodeB = graph.AddNode(taskB, {nodeA});
    // And then I want A to depend on B.
    // I can't do that with `AddNode`.
    
    // So the only way to have a cycle is if the graph is corrupted or modified internally.
    // OR if I use `const_cast` to modify the nodes returned by `GetNodes()`.
    // Let's do that for the sake of testing the executor's robustness.
    
    const auto& nodes = graph.GetNodes();
    auto& mutableNodeA = const_cast<JobGraphNode&>(*nodes[nodeA]);
    auto& mutableNodeB = const_cast<JobGraphNode&>(*nodes[nodeB]);
    
    // Add B as dependency to A
    mutableNodeA.m_dependencies.push_back(nodes[nodeB]);
    mutableNodeB.m_dependents.push_back(nodes[nodeA]);
    
    // Now we have A -> B -> A
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsError());
}

TEST(JobSystemTest, ChainedMessagePassing) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;
    
    std::vector<std::string> log;
    
    // Job A: Produces Msg1
    auto jobA = [&](JobContext&, PortOut<TestMessage>& out) {
        log.push_back("A");
        out.Send(TestMessage{1, "from A"});
        return Error{};
    };
    int nodeA = graph.AddMessageNode(jobA, {});
    
    // Job B: Consumes Msg1, Produces Msg2
    auto jobB = [&](JobContext&, PortIn<TestMessage>& in, PortOut<AnotherMessage>& out) {
        in.Handle([&](const TestMessage& msg) {
            log.push_back("B received " + msg.text);
        });
        out.Send(AnotherMessage{2.0f});
        return Error{};
    };
    std::vector<int> depsB = {nodeA};
    int nodeB = graph.AddMessageNode(jobB, depsB);
    
    // Job C: Consumes Msg2
    auto jobC = [&](JobContext&, PortIn<AnotherMessage>& in) {
        in.Handle([&](const AnotherMessage& msg) {
            log.push_back("C received " + std::to_string(static_cast<int>(msg.data)));
        });
        return Error{};
    };
    std::vector<int> depsC = {nodeB};
    int nodeC = graph.AddMessageNode(jobC, depsC);
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());
    
    ASSERT_EQ(log.size(), 3);
    EXPECT_EQ(log[0], "A");
    EXPECT_EQ(log[1], "B received from A");
    EXPECT_EQ(log[2], "C received 2");
}

TEST(JobSystemTest, MultipleProducersSingleConsumer) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;
    
    std::atomic<int> sum = 0;
    
    // Producer 1
    auto prod1 = [](JobContext&, PortOut<TestMessage>& out) {
        out.Send(TestMessage{10, "p1"});
        return Error{};
    };
    int nodeP1 = graph.AddMessageNode(prod1, {});
    
    // Producer 2
    auto prod2 = [](JobContext&, PortOut<TestMessage>& out) {
        out.Send(TestMessage{20, "p2"});
        return Error{};
    };
    int nodeP2 = graph.AddMessageNode(prod2, {});
    
    // Consumer
    auto consumer = [&](JobContext&, PortIn<TestMessage>& in) {
        in.Handle([&](const TestMessage& msg) {
            sum += msg.value;
        });
        return Error{};
    };
    std::vector<int> deps = {nodeP1, nodeP2};
    int nodeC = graph.AddMessageNode(consumer, deps);
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());
    
    EXPECT_EQ(sum, 30);
}
