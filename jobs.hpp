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
    struct message_node_param_trait {
        static constexpr bool is_valid = false;
    };

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

    template <typename Tuple>
    struct tuple_drop_first_second;

    template <typename First, typename Second, typename... Rest>
    struct tuple_drop_first_second<std::tuple<First, Second, Rest...>> {
        using type = std::tuple<Rest...>;
    };

    template <typename T>
    struct function_traits<T, std::enable_if_t<std::is_class_v<T>>> : function_traits<decltype(&T::operator()), void> {};

    // Make a concept for messages
    template <typename T>
    concept MessageConcept = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

    // Make a concept for signals
    template <typename T>
    concept SignalConcept = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

    class IMessagePort {    
    public:
        virtual void Clear() = 0;
        virtual std::type_index GetMessageType() const = 0;

        virtual ~IMessagePort() = default;
    };

    template <MessageConcept T>
    class MessagePort final : public IMessagePort {
    public:
        std::shared_mutex m_mutex;
        std::vector<T> m_messages;

        void Send(T message) {
            std::unique_lock lock(m_mutex);
            m_messages.push_back(std::move(message));
        }

        inline void HandleNoLock(std::invocable<T const&> auto&& handler) {
            std::shared_lock lock(m_mutex);
            for (const auto& message : m_messages) {
                handler(message);
            }
        }

        inline void HandlePipeNoLock(std::invocable<std::span<T>> auto&& handler) {
            std::shared_lock lock(m_mutex);
            handler(std::span<T>(m_messages.data(), m_messages.size()));
        }

        inline void HandlePipeSingleNoLock(std::invocable<T&> auto&& handler) {
            std::shared_lock lock(m_mutex);
            if (!m_messages.empty()) {
                handler(m_messages[0]);
            }
        }

        inline void Handle(std::invocable<T const&> auto&& handler) {
            std::shared_lock lock(m_mutex);
            HandleNoLock(std::move(handler));
        }

        inline void HandlePipe(std::invocable<std::span<T>> auto&& handler) {
            std::shared_lock lock(m_mutex);
            HandlePipeNoLock(std::move(handler));
        }

        inline void HandlePipeSingle(std::invocable<T&> auto&& handler) {
            std::shared_lock lock(m_mutex);
            HandlePipeSingleNoLock(std::move(handler));
        }

        std::type_index GetMessageType() const override {
            return typeid(T);
        }

        void Clear() override {
            std::unique_lock lock(m_mutex);
            m_messages.clear();
        }
    };

    template <MessageConcept T>
    struct In {
        MessagePort<T>* m_lane;
        std::shared_lock<std::shared_mutex> m_lock;

        In() = default;
        In(MessagePort<T>* lane) : m_lane(lane), m_lock(lane->m_mutex) {}

        inline void Handle(std::invocable<T const&> auto&& handler) {
            m_lane->HandleNoLock(std::move(handler));
        }

        T const& operator*() const {
            return m_lane->m_messages.front();
        }

        T const* operator->() const {
            return &m_lane->m_messages.front();
        }
    };

    template <MessageConcept T>
    struct Out {
        MessagePort<T>* m_lane;
        Out() = default;
        Out(MessagePort<T>* lane) : m_lane(lane) {}

        inline void Send(T message) {
            m_lane->Send(std::move(message));
        }
    };

    template <MessageConcept T, int Priority = 0>
    struct Pipe {
        static constexpr int kPriority = Priority;

        MessagePort<T>* m_port;
        std::shared_lock<std::shared_mutex> m_lock;

        Pipe() = default;
        Pipe(MessagePort<T>* port) : m_port(port), m_lock(port->m_mutex) {}

        inline void Handle(std::invocable<std::span<T>> auto&& handler) {
            m_port->HandlePipeNoLock(std::move(handler));
        }

        inline void HandleSingle(std::invocable<T&> auto&& handler) {
            m_port->HandlePipeSingleNoLock(std::move(handler));
        }
    };

    template <typename T>
    struct TraitIsPipe { static constexpr bool value = false; };
    template <typename T, int Priority>
    struct TraitIsPipe<Pipe<T, Priority>> { static constexpr bool value = true; };

    template <typename T>
    struct PipeGetType { using type = std::void_t<>; };
    template <typename T, int Priority>
    struct PipeGetType<Pipe<T, Priority>> { using type = T; };

    enum class NodeParamType {
        PORT_IN,
        PORT_OUT,
        PIPE
    };

    template <typename T>
    struct message_node_param_trait<In<T>> {
        using message_type = T; 
        static constexpr NodeParamType type = NodeParamType::PORT_IN;
        static constexpr bool is_valid = true;
    };

    template <typename T>
    struct message_node_param_trait<Out<T>> {
        using message_type = T;
        static constexpr NodeParamType type = NodeParamType::PORT_OUT;
        static constexpr bool is_valid = true;
    };

    template <typename T, int Priority>
    struct message_node_param_trait<Pipe<T, Priority>> {
        using message_type = T;
        static constexpr NodeParamType type = NodeParamType::PIPE;
        static constexpr bool is_valid = true;
    };

    template <typename T>
    using MsgIn = In<T>;

    template <typename T>
    using MsgOut = Out<T>;

    class MessageBus {
    private:
        std::unordered_map<std::type_index, std::unique_ptr<IMessagePort>> m_ports;

    public:
        virtual ~MessageBus() = default;

        void Send(MessageConcept auto message) const {
            if (auto it = m_ports.find(typeid(message)); it != m_ports.end()) {
                auto lane = dynamic_cast<MessagePort<std::decay_t<decltype(message)>>*>(it->second.get());
                OKAMI_ASSERT(lane != nullptr, "Message port type mismatch");
                lane->Send(std::move(message));
            }
        }

        template <MessageConcept T>
        void EnsurePort() {
            std::type_index typeIdx = typeid(T);
            if (m_ports.find(typeIdx) == m_ports.end()) {
                m_ports[typeIdx] = std::make_unique<MessagePort<T>>();
            }
        }

        template <MessageConcept T>
        MessagePort<std::decay_t<T>>* GetPort() const {
            std::type_index typeIdx = typeid(T);
            if (auto it = m_ports.find(typeIdx); it != m_ports.end()) {
                return dynamic_cast<MessagePort<std::decay_t<T>>*>(it->second.get());
            }
            return nullptr;
        }

        // Helper to create and bind a port
        template <typename T>
        auto GetWrapper() {
            using trait = message_node_param_trait<std::remove_reference_t<T>>;
            using msg_t = typename trait::message_type;
            if constexpr (trait::type == NodeParamType::PORT_IN) {
                In<msg_t> port;
                port.m_lane = GetPort<msg_t>();
                return port;
            } else if constexpr (trait::type == NodeParamType::PORT_OUT) {
                Out<msg_t> port;
                port.m_lane = GetPort<msg_t>();
                return port;
            } else if constexpr (trait::type == NodeParamType::PIPE) {
                constexpr auto kPriority = T::kPriority; 
                Pipe<msg_t, kPriority> pipe;
                pipe.m_port = GetPort<msg_t>();
                return pipe;
            }
        }

        template <MessageConcept T>
        Out<T> GetPortOut() {
            return GetWrapper<Out<T>>();
        }

        template <MessageConcept T>
        In<T> GetPortIn() {
            return GetWrapper<In<T>>();
        }

        template <MessageConcept T>
        Pipe<T> GetPipe() {
            Pipe<T> pipe;
            pipe.m_port = GetPort<T>();
            return pipe;
        }

        template <MessageConcept T>
        void Handle(std::invocable<T const&> auto&& handler) {
            if (auto lane = GetPort<T>()) {
                lane->Handle(handler);
            }
        }

        void Clear() {
            for (auto& [typeIdx, port] : m_ports) {
                port->Clear();
            }
        }
    };

    struct JobContext {
        MessageBus& m_messageBus;
        // Context data for jobs can be added here
    };
    
    struct JobGraphNode {
        int m_id = -1;
        std::vector<std::shared_ptr<JobGraphNode>> m_dependencies;
        std::vector<std::shared_ptr<JobGraphNode>> m_dependents;
        std::function<Error(JobContext&)> m_task = nullptr;
        std::atomic<int> m_pendingDependencies{ 0 };
        std::function<void(MessageBus&)> m_portEnsure = nullptr;
    
        // Helper to populate message types
        template <typename msg_args_tuple>
        void AddPortEnsure() {
            m_portEnsure = [](MessageBus& bus) {
                auto ensure_helper = [&]<typename... Ts>(std::tuple<Ts...>*) {
                    ([&]<typename PortT>() {
                        using msg_t = typename message_node_param_trait<std::remove_reference_t<PortT>>::message_type;
                        bus.EnsurePort<msg_t>();
                    }.template operator()<Ts>(), ...);
                };
                ensure_helper(static_cast<msg_args_tuple*>(nullptr));
            };
        }
    };

    template <typename msg_args_tuple>
    msg_args_tuple GetPortTuple(MessageBus& bus) {
        return []<typename... Ts>(std::tuple<Ts...>*, MessageBus& bus) {
            return std::tuple<Ts...>{
                bus.GetWrapper<Ts>()...
            };
        }(static_cast<msg_args_tuple*>(nullptr), bus);
    }
    
    class JobGraph {
    private:
        bool m_finalized = false;
        std::vector<std::shared_ptr<JobGraphNode>> m_nodes;

        struct PipeGroup {
            struct Internal {
                std::shared_ptr<JobGraphNode> m_node;
                int m_priority = 0;
            };

            std::shared_ptr<JobGraphNode> m_pipeStart;
            std::shared_ptr<JobGraphNode> m_pipeEnd;
            std::vector<Internal> m_nodes;
        };

        std::unordered_map<std::type_index, PipeGroup> m_message_pipes;

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

        PipeGroup& EnsurePipe(std::type_index type) {
            if (auto it = m_message_pipes.find(type); it == m_message_pipes.end()) {
                auto pipe = PipeGroup{
                    .m_pipeStart = AddNodeInternal(nullptr, {}), 
                    .m_pipeEnd = AddNodeInternal(nullptr, {})
                };
                return m_message_pipes.emplace_hint(it, type, std::move(pipe))->second;
            } else {
                return it->second;
            }
        }

        void AddEdgeInternal(std::shared_ptr<JobGraphNode> from, std::shared_ptr<JobGraphNode> to) {
            from->m_dependents.push_back(to);
            to->m_dependencies.push_back(from);
            to->m_pendingDependencies = static_cast<int>(to->m_dependencies.size());
        }

        template <typename PortWrapperT>
        void ConnectBarrierIn(std::shared_ptr<JobGraphNode> node) {
            if constexpr (message_node_param_trait<PortWrapperT>::type == NodeParamType::PORT_IN) {
                auto& pipe = EnsurePipe(typeid(message_node_param_trait<PortWrapperT>::message_type));
                AddEdgeInternal(pipe.m_pipeEnd, node);
            }
        }

        template <typename PortWrapperT>
        void ConnectBarrierOut(std::shared_ptr<JobGraphNode> node) {
            if constexpr (message_node_param_trait<PortWrapperT>::type == NodeParamType::PORT_OUT) {
                auto& pipe = EnsurePipe(typeid(message_node_param_trait<PortWrapperT>::message_type));
                AddEdgeInternal(node, pipe.m_pipeStart);
            }
        }

        template <typename PortWrapperT>
        void ConnectBarrierPipe(std::shared_ptr<JobGraphNode> node) {
            if constexpr (message_node_param_trait<PortWrapperT>::type == NodeParamType::PIPE) {
                auto& pipe = EnsurePipe(typeid(message_node_param_trait<PortWrapperT>::message_type));
                pipe.m_nodes.push_back(PipeGroup::Internal{
                    .m_node = node,
                    .m_priority = PortWrapperT::kPriority
                });
            }
        }

        template <typename msg_args_tuple>
        void ConnectAllDependencies(std::shared_ptr<JobGraphNode> node) {
            []<std::size_t... Is>(std::index_sequence<Is...>, JobGraph& graph, std::shared_ptr<JobGraphNode> node) {
                (graph.ConnectBarrierIn<std::tuple_element_t<Is, msg_args_tuple>>(node), ...);
                (graph.ConnectBarrierOut<std::tuple_element_t<Is, msg_args_tuple>>(node), ...);
                (graph.ConnectBarrierPipe<std::tuple_element_t<Is, msg_args_tuple>>(node), ...);
            }(std::make_index_sequence<std::tuple_size_v<msg_args_tuple>>{}, *this, node);
        }

    public:
        int AddNode(std::function<Error(JobContext&)> task, std::span<int const> dependencies = {}) {
            auto node = AddNodeInternal(task, dependencies);
            return node->m_id;
        }

        void AddDepencyEdge(int from, int to) {
            if (from < 0 || from >= static_cast<int>(m_nodes.size()) || to < 0 || to >= static_cast<int>(m_nodes.size())) {
                throw std::out_of_range("Invalid node ID(s) for dependency edge");
            }
            AddEdgeInternal(m_nodes[from], m_nodes[to]);
        }

        // New AddMessageNode method
        template <typename Callable>
        int AddMessageNode(Callable task, std::span<int const> dependencies = {}) {
            // For all of the In arguments, add the 
            using args_tuple = typename function_traits<Callable>::args_tuple;
            using msg_args_tuple = typename tuple_drop_first<args_tuple>::type;

            static_assert(std::is_same_v<std::tuple_element_t<0, args_tuple>, JobContext&>,
                          "First argument of the task must be JobContext&");

            // Create the node
            auto node = AddNodeInternal([task](JobContext& ctx) mutable -> Error {
                auto ports_tuple = GetPortTuple<msg_args_tuple>(ctx.m_messageBus);
                return std::apply([&](auto&... ports) { return task(ctx, std::move(ports)...); }, ports_tuple);
            }, dependencies);

            ConnectAllDependencies<msg_args_tuple>(node);

            // Populate message types
            node->template AddPortEnsure<msg_args_tuple>();
            return node->m_id;
        }

        inline const std::vector<std::shared_ptr<JobGraphNode>>& GetNodes() const {
            return m_nodes;
        }

        void Finalize();

        inline bool IsFinalized() const {
            return m_finalized;
        }
    };

    class IJobGraphExecutor {
    public:
        virtual ~IJobGraphExecutor() = default;

        virtual Error Execute(JobGraph& graph, MessageBus& bus) = 0;
    };

    class DefaultJobGraphExecutor : public IJobGraphExecutor {
    public:
        Error Execute(JobGraph& graph, MessageBus& bus) override;
    };
}