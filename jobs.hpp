#pragma once

#include "common.hpp"

#include <atomic>
#include <span>
#include <typeindex>
#include <any>
#include <shared_mutex>
#include <functional>

namespace okami {
    // Message type trait
    template <typename T>
    struct message_type_trait;

    // Function traits for extracting parameter types
    template <typename T, typename = void>
    struct function_traits;

    template <typename R, typename... Args>
    struct function_traits<R(*)(Args...), void> {
        using return_type = R;
        using args_tuple = std::tuple<Args...>;
    };

    template <typename R, typename T, typename... Args>
    struct function_traits<R (T::*)(Args...), void> {
        using return_type = R;
        using args_tuple = std::tuple<Args...>;
    };

    template <typename R, typename T, typename... Args>
    struct function_traits<R (T::*)(Args...) const, void> {
        using return_type = R;
        using args_tuple = std::tuple<Args...>;
    };

    // Tuple drop first helper
    template <typename Tuple>
    struct tuple_drop_first;

    template <typename First, typename... Rest>
    struct tuple_drop_first<std::tuple<First, Rest...>> {
        using type = std::tuple<Rest...>;
    };

    template <typename T>
    struct function_traits<T, std::enable_if_t<std::is_class_v<T>>> : function_traits<decltype(&T::operator()), void> {};

    // Make a concept for copyable messages
    template <typename T>
    concept CopyableMessage = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

    template <CopyableMessage T>
    struct MessagePort {
        std::shared_mutex m_mutex;
        std::vector<T> m_messages;

        void Send(const T& message) {
            std::unique_lock lock(m_mutex);
            m_messages.push_back(message);
        }

        void Handle(std::invocable<T const&> auto&& handler) {
            std::shared_lock lock(m_mutex);
            for (const auto& message : m_messages) {
                handler(message);
            }
        }
    };

    template <CopyableMessage T>
    struct PortIn {
        std::shared_ptr<MessagePort<T>> m_lane;

        PortIn() = default;
        PortIn(std::shared_ptr<MessagePort<T>> lane) : m_lane(std::move(lane)) {}

        inline void Handle(std::invocable<T const&> auto&& handler) {
            m_lane->Handle(handler);
        }
    };

    template <CopyableMessage T>
    struct PortOut {
        std::shared_ptr<MessagePort<T>> m_lane;

        PortOut() = default;
        PortOut(std::shared_ptr<MessagePort<T>> lane) : m_lane(std::move(lane)) {}

        inline void Send(const T& message) {
            m_lane->Send(message);
        }
    };

    template <typename T>
    struct message_type_trait<PortIn<T>> {
        using message_type = T;
        static constexpr bool is_input = true;
    };

    template <typename T>
    struct message_type_trait<PortOut<T>> {
        using message_type = T;
        static constexpr bool is_input = false;
    };

    template <typename T>
    using MsgIn = PortIn<T>;

    template <typename T>
    using MsgOut = PortOut<T>;

    class MessageBus2 {
    private:
        std::unordered_map<std::type_index, std::any> m_lanes;

    public:
        virtual ~MessageBus2() = default;

        void Send(CopyableMessage auto const& message) const {
            if (auto it = m_lanes.find(typeid(message)); it != m_lanes.end()) {
                auto lane = std::any_cast<std::shared_ptr<MessagePort<std::decay_t<decltype(message)>>>>(it->second);
                lane->Send(message);
            }
        }

        void EnsureLane(CopyableMessage auto const& message) {
            std::type_index typeIdx = typeid(message);
            if (m_lanes.find(typeIdx) == m_lanes.end()) {
                m_lanes[typeIdx] = std::make_shared<MessagePort<std::decay_t<decltype(message)>>>();
            }
        }

        template <CopyableMessage T>
        void EnsureLane() {
            std::type_index typeIdx = typeid(T);
            if (m_lanes.find(typeIdx) == m_lanes.end()) {
                m_lanes[typeIdx] = std::make_shared<MessagePort<T>>();
            }
        }

        template <CopyableMessage T>
        std::shared_ptr<MessagePort<std::decay_t<T>>> GetLane() const {
            std::type_index typeIdx = typeid(T);
            if (auto it = m_lanes.find(typeIdx); it != m_lanes.end()) {
                return std::any_cast<std::shared_ptr<MessagePort<std::decay_t<T>>>>(it->second);
            }
            return nullptr;
        }

        // Helper to create and bind a port
        template <typename T>
        auto CreatePort() {
            using trait = message_type_trait<std::remove_reference_t<T>>;
            using msg_t = typename trait::message_type;
            if constexpr (trait::is_input) {
                PortIn<msg_t> port;
                port.m_lane = GetLane<msg_t>();
                return port;
            } else {
                PortOut<msg_t> port;
                port.m_lane = GetLane<msg_t>();
                return port;
            }
        }
    };

    struct JobContext {
        MessageBus2& m_messageBus;
        // Context data for jobs can be added here
    };
    
    struct JobGraphNode {
        int m_id = -1;
        std::vector<std::shared_ptr<JobGraphNode>> m_dependencies;
        std::vector<std::shared_ptr<JobGraphNode>> m_dependents;
        std::function<Error(JobContext&)> m_task = nullptr;
        std::atomic<int> m_pendingDependencies{ 0 };
        std::function<void(MessageBus2&)> m_portEnsure = nullptr;
    
        // Helper to populate message types
        template <typename Callable>
        void AddPortEnsure() {
            using args_tuple = typename function_traits<Callable>::args_tuple;
            static_assert(std::tuple_size_v<args_tuple> >= 1, "Callable must have at least one parameter");
            using first_arg = std::tuple_element_t<0, args_tuple>;
            static_assert(std::is_same_v<std::remove_reference_t<first_arg>, JobContext>, "First parameter must be JobContext&");
            using msg_args_tuple = typename tuple_drop_first<args_tuple>::type;

            m_portEnsure = [](MessageBus2& bus) {
                auto ensure_helper = [&]<typename... Ts>(std::tuple<Ts...>*) {
                    ([&]<typename PortT>() {
                        using trait = message_type_trait<std::remove_reference_t<PortT>>;
                        using msg_t = typename trait::message_type;
                        bus.EnsureLane<msg_t>();
                    }.template operator()<Ts>(), ...);
                };
                ensure_helper(static_cast<msg_args_tuple*>(nullptr));
            };
        }
    };
    
    class JobGraph {
    private:
        std::vector<std::shared_ptr<JobGraphNode>> m_nodes;

        std::shared_ptr<JobGraphNode> AddNodeInternal(
            std::function<Error(JobContext&)> task, 
            std::span<int const> dependencies) {

            auto node = std::make_shared<JobGraphNode>();
            node->m_id = static_cast<int>(m_nodes.size());
            node->m_task = task;

            // Set up dependencies
            for (int depId : dependencies) {
                if (depId < 0 || depId >= static_cast<int>(m_nodes.size())) {
                    throw std::out_of_range("Invalid dependency node ID: " + std::to_string(depId));
                }
                auto& depNode = m_nodes[depId];
                node->m_dependencies.push_back(depNode);
                depNode->m_dependents.push_back(node);
                node->m_pendingDependencies = static_cast<int>(node->m_dependencies.size());
            }

            m_nodes.push_back(node);
            return node;
        }

    public:
        int AddNode(std::function<Error(JobContext&)> task, std::span<int const> dependencies) {
            auto node = AddNodeInternal(task, dependencies);
            return node->m_id;
        }

        // New AddMessageNode method
        template <typename Callable>
        int AddMessageNode(Callable task, std::span<int const> dependencies) {
            // Ensure the callable signature is correct
            static_assert(std::invocable<Callable, JobContext&> || std::tuple_size_v<typename function_traits<Callable>::args_tuple> > 1,
                          "Callable must be invocable with JobContext& and message ports");

            // Create the node
            auto node = AddNodeInternal([task](JobContext& ctx) mutable -> Error {
                using args_tuple = typename function_traits<Callable>::args_tuple;
                using msg_args_tuple = typename tuple_drop_first<args_tuple>::type;

                // Create the ports
                auto ports_tuple = []<typename... Ts>(std::tuple<Ts...>*, MessageBus2& bus) {
                    return std::tuple<std::conditional_t<message_type_trait<std::remove_reference_t<Ts>>::is_input, 
                                                         PortIn<typename message_type_trait<std::remove_reference_t<Ts>>::message_type>, 
                                                         PortOut<typename message_type_trait<std::remove_reference_t<Ts>>::message_type>>... >{
                        bus.GetLane<typename message_type_trait<std::remove_reference_t<Ts>>::message_type>()...
                    };
                }(static_cast<msg_args_tuple*>(nullptr), ctx.m_messageBus);

                return std::apply([&](auto&... ports) { return task(ctx, ports...); }, ports_tuple);
            }, dependencies);

            // Populate message types
            node->template AddPortEnsure<Callable>();
            return node->m_id;
        }

        const std::vector<std::shared_ptr<JobGraphNode>>& GetNodes() const {
            return m_nodes;
        }

    private:
    };

    class IJobGraphExecutor {
    public:
        virtual ~IJobGraphExecutor() = default;

        virtual Error Execute(JobGraph& graph, MessageBus2& bus) = 0;
    };

    class DefaultJobGraphExecutor : public IJobGraphExecutor {
    public:
        Error Execute(JobGraph& graph, MessageBus2& bus) override;
    };
}