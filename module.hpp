#pragma once

#include <vector>
#include <typeindex>
#include <any>
#include <queue>
#include <mutex>
#include <optional>

#include "common.hpp"
#include "log.hpp"
#include "jobs.hpp"

namespace okami {
    template <SignalConcept T>
    class ISignalHandler {
    public:
        virtual ~ISignalHandler() = default;

        // Should be thread safe
        virtual void Send(T message) = 0;
    };

    template <SignalConcept T>
    class DefaultSignalHandler : public ISignalHandler<T> {
    private:
        std::mutex m_mutex;
        std::queue<T> m_messages;

    public:
        void Send(T message) override {
            std::lock_guard lock(m_mutex);
            m_messages.push(std::move(message));
        }

        void Handle(std::invocable<T> auto&& handler) {
            std::lock_guard lock(m_mutex);
            while (!m_messages.empty()) {
                handler(std::move(m_messages.front()));
                m_messages.pop();
            }
        }

        void Clear() {
            Handle([](T) {});
        }
    };

    // Counts the number of times a message has been received
    template <SignalConcept T>
    class CountSignalHandler : public ISignalHandler<T> {
    public:
        std::atomic<size_t> m_count{ 0 };

        void Send(T message) override {
            m_count += 1;
        }

        size_t FetchAndReset() {
            return m_count.exchange(0);
        }
    };

    class InterfaceCollection final {
    public:
        virtual ~InterfaceCollection();

        template <typename T>
        void Register(T* interfacePtr) {
            m_interfaces.emplace(std::type_index(typeid(T)), interfacePtr);
        }

        std::optional<std::any> QueryType(std::type_info const& type) const;

        template <typename T>
        T* Query() const {
            if (auto result = QueryType(typeid(T)); result) {
                return std::any_cast<T*>(*result);
            }
            else {
                return nullptr;
            }
        }

        template <typename T, typename Callable>
        void ForEachInterface(Callable&& func) {
            std::type_index typeIdx = std::type_index(typeid(T));
            auto range = m_interfaces.equal_range(typeIdx);
            for (auto it = range.first; it != range.second; ++it) {
                func(std::any_cast<T*>(it->second));
            }
        }

        auto begin();
        auto end();
        auto cbegin() const;
        auto cend() const;

        template <SignalConcept T>
        void RegisterSignalHandler(ISignalHandler<T>* handler) {
            Register<ISignalHandler<T>>(handler);
        }

        template <SignalConcept T>
        void SendSignal(T message) const {
            auto handler = Query<ISignalHandler<T>>();
            if (handler) {
                handler->Send(std::move(message));
            } else {
                OKAMI_LOG_WARNING("No signal handler registered for message type: " + std::string{typeid(T).name()});
            }
        }

    protected:
        std::unordered_multimap<std::type_index, std::any> m_interfaces;
    };

    struct InitContext {
        MessageBus& m_messages;
        InterfaceCollection& m_interfaces;
    };

    struct Time {
		double m_deltaTime;
        double m_nextFrameTime;
		double m_lastFrameTime;
		size_t m_nextFrame;
	};

    struct BuildGraphParams {
    };

    class IIOModule {
    public:
        virtual ~IIOModule() = default;
        virtual Error IOProcess(InterfaceCollection& interfaces) = 0;
    };

    class IGUIModule {
    public:
        virtual ~IGUIModule() = default;
        virtual Error MessagePump(InterfaceCollection& interfaces) = 0;
    };

    class EngineModule {
    private:
        std::vector<std::unique_ptr<EngineModule>> m_submodules;
        bool b_started = false;
        bool b_shutdown = false;

        // Sets if children should have their BuildGraph called automatically
        bool b_children_build_update_graph = true;
        bool b_children_process_startup = true;

    protected:
        inline void SetChildrenProcessFrame(bool enable) {
            b_children_build_update_graph = enable;
        }

        inline void SetChildrenProcessStartup(bool enable) {
            b_children_process_startup = enable;
        }

        virtual Error RegisterImpl(InterfaceCollection&) { return {}; }

        virtual Error StartupImpl(InitContext const&) { return {}; }
        virtual void ShutdownImpl(InitContext const&) { }

        virtual Error BuildGraphImpl(JobGraph&, BuildGraphParams const& = {}) { return {}; }

        virtual Error SendMessagesImpl(MessageBus&) { return {}; }
        virtual Error ReceiveMessagesImpl(MessageBus&) { return {}; }

    public:
		auto begin();
		auto end();
		auto begin() const;
		auto end() const;

        template <typename T, typename... TArgs>
        T* CreateChild(TArgs&&... a) {
            auto ptr = std::make_unique<T>(std::forward<TArgs>(a)...);
            auto result = ptr.get();
            m_submodules.emplace_back(std::move(ptr));
            return result;
        }

        template <typename FactoryT, typename... TArgs>
        auto CreateChildFromFactory(FactoryT factory = FactoryT{}, TArgs&&... args) {
            auto ptr = factory(std::forward<TArgs>(args)...);
            auto result = ptr.get();
            m_submodules.emplace_back(std::move(ptr));
            return result;
        }

        Error Register(InterfaceCollection&);
        Error Startup(InitContext const&);

        Error SendMessages(MessageBus&);
        Error ReceiveMessages(MessageBus&);

        Error BuildGraph(JobGraph&, BuildGraphParams const& = {});

        void Shutdown(InitContext const& a);

        virtual std::string GetName() const;

        virtual ~EngineModule();
    };
}