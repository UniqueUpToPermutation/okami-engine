#include "jobs.hpp"

#include <unordered_set>
#include <queue>

using namespace okami;

Error ExecuteSerial(
    std::shared_ptr<JobGraphNode> node,
    JobContext& context) {
    Error result;
    
    // Execute the task
    Error err = node->m_task(context);

    if (err.IsOk()) {
        // Notify dependents
        for (const auto& dependent : node->m_dependents) {
            if (--dependent->m_pendingDependencies == 0) {
                // Schedule dependent for execution
                err += ExecuteSerial(dependent, context);
            }
        }
    }

    return err;
}

Error okami::DefaultJobGraphExecutor::Execute(JobGraph& graph, MessageBus2& bus) {
    JobContext context{ .m_messageBus = bus };
    Error err;

    // Initialize pending dependencies count
    for (const auto& node : graph.GetNodes()) {
        node->m_pendingDependencies = static_cast<int>(node->m_dependencies.size());

        // Make sure the message ports are set up correctly
        if (node->m_portEnsure) {
            node->m_portEnsure(bus);
        }
    }

    // Use a queue for BFS execution
    std::queue<std::shared_ptr<JobGraphNode>> ready;
    int executedCount = 0;
    for (const auto& node : graph.GetNodes()) {
        if (node->m_pendingDependencies == 0) {
            ready.push(node);
        }
    }

    while (!ready.empty()) {
        auto node = ready.front();
        ready.pop();
        executedCount++;

        // Execute the job
        Error jobErr = node->m_task(context);
        if (!jobErr.IsOk()) {
            err += jobErr;
            continue; // Continue with other jobs even if one fails?
        }

        // Update dependents
        for (const auto& dependent : node->m_dependents) {
            if (--dependent->m_pendingDependencies == 0) {
                ready.push(dependent);
            }
        }
    }

    OKAMI_ERROR_RETURN_IF(executedCount != graph.GetNodes().size(),
        "Cycle detected or unreachable nodes in JobGraph");

    return err;
}