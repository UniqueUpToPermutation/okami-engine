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

// Test MessageBus
TEST(JobSystemTest, MessageBusEnsureLane) {
    MessageBus bus;
    
    // Ensure lane for TestMessage
    bus.EnsurePort<TestMessage>();
    
    // Get the lane - note: GetPort is const, but bus is non-const, should work
    auto lane = bus.GetPort<TestMessage>();
    ASSERT_NE(lane, nullptr);
    
    // Ensure another lane
    bus.EnsurePort<AnotherMessage>();
    auto anotherLane = bus.GetPort<AnotherMessage>();
    ASSERT_NE(anotherLane, nullptr);
}

TEST(JobSystemTest, MessageBusSendAndReceive) {
    MessageBus bus;
    bus.EnsurePort<TestMessage>();
    
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
    auto producer = [&](JobContext& ctx, Out<TestMessage> out) {
        out.Send(TestMessage{42, "produced"});
        return Error{};
    };
    int producerNode = graph.AddMessageNode(producer);
    
    // Consumer job
    auto consumer = [&](JobContext& ctx, In<TestMessage> in) {
        in.Handle([&](const TestMessage& msg) {
            received.push_back(msg);
        });
        return Error{};
    };
    int consumerNode = graph.AddMessageNode(consumer);
    
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
    
    auto taskA = [](JobContext&) { return Error{}; };
    int nodeA = graph.AddNode(taskA);
    
    auto taskB = [](JobContext&) { return Error{}; };
    int nodeB = graph.AddNode(taskB);

    graph.AddDepencyEdge(nodeA, nodeB);
    graph.AddDepencyEdge(nodeB, nodeA);
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsError());
}

TEST(JobSystemTest, ChainedMessagePassing) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;
    
    std::vector<std::string> log;
    
    // Job A: Produces Msg1
    auto jobA = [&](JobContext&, Out<TestMessage> out) {
        log.push_back("A");
        out.Send(TestMessage{1, "from A"});
        return Error{};
    };
    int nodeA = graph.AddMessageNode(jobA);
    
    // Job B: Consumes Msg1, Produces Msg2
    auto jobB = [&](JobContext&, In<TestMessage> in, Out<AnotherMessage> out) {
        in.Handle([&](const TestMessage& msg) {
            log.push_back("B received " + msg.text);
        });
        out.Send(AnotherMessage{2.0f});
        return Error{};
    };
    int nodeB = graph.AddMessageNode(jobB);
    
    // Job C: Consumes Msg2
    auto jobC = [&](JobContext&, In<AnotherMessage> in) {
        in.Handle([&](const AnotherMessage& msg) {
            log.push_back("C received " + std::to_string(static_cast<int>(msg.data)));
        });
        return Error{};
    };
    int nodeC = graph.AddMessageNode(jobC);
    
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
    auto prod1 = [](JobContext&, Out<TestMessage> out) {
        out.Send(TestMessage{10, "p1"});
        return Error{};
    };
    int nodeP1 = graph.AddMessageNode(prod1);
    
    // Producer 2
    auto prod2 = [](JobContext&, Out<TestMessage> out) {
        out.Send(TestMessage{20, "p2"});
        return Error{};
    };
    int nodeP2 = graph.AddMessageNode(prod2);
    
    // Consumer
    auto consumer = [&](JobContext&, In<TestMessage> in) {
        in.Handle([&](const TestMessage& msg) {
            sum += msg.value;
        });
        return Error{};
    };
    int nodeC = graph.AddMessageNode(consumer);
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());
    
    EXPECT_EQ(sum, 30);
}

TEST(JobSystemTest, PipeTest) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;

    struct TestPipe {};

    auto prod2 = [](JobContext&, Pipe<TestPipe, 0>, Out<TestMessage> outMsg) {
        outMsg.Send(TestMessage{20, "p2"});
        return Error{};
    };
    int nodeP2 = graph.AddMessageNode(prod2);

    auto prod1 = [](JobContext&, Pipe<TestPipe, 1>, Out<TestMessage> outMsg) {
        outMsg.Send(TestMessage{10, "p1"});
        return Error{};
    };
    int nodeP1 = graph.AddMessageNode(prod1);
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());

    auto msg = bus.GetPort<TestMessage>();

    EXPECT_EQ(msg->m_messages.size(), 2);
    EXPECT_EQ(msg->m_messages[0].value, 10);
    EXPECT_EQ(msg->m_messages[0].text, "p1");
    EXPECT_EQ(msg->m_messages[1].value, 20);
    EXPECT_EQ(msg->m_messages[1].text, "p2");
}

TEST(JobSystemTest, PipeTest2) {
    JobGraph graph;
    MessageBus bus;
    DefaultJobGraphExecutor executor;

    bus.EnsurePort<TestMessage>();
    bus.Send(TestMessage{.value = 0, .text = ""});

    auto prod2 = [](JobContext&, Pipe<TestMessage, 0> msg) {
        msg->text = "2";
        msg->value += 1;
        return Error{};
    };
    int nodeP2 = graph.AddMessageNode(prod2);

    auto prod1 = [](JobContext&, Pipe<TestMessage, 1> msg) {
        msg->text = "1";
        msg->value += 1;
        return Error{};
    };
    int nodeP1 = graph.AddMessageNode(prod1);
    
    Error result = executor.Execute(graph, bus);
    EXPECT_TRUE(result.IsOk());

    auto msg = bus.GetPort<TestMessage>();

    EXPECT_EQ(msg->m_messages.size(), 1);
    EXPECT_EQ(msg->m_messages[0].text, "2");
    EXPECT_EQ(msg->m_messages[0].value, 2);
}