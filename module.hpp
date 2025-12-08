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
    template <MoveableSignal T>
    class ISignalHandler {
    public:
        virtual ~ISignalHandler() = default;

        // Should be thread safe
        virtual void Send(T message) = 0;
    };

    template <MoveableSignal T>
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
    template <MoveableSignal T>
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
            m_interfaces[std::type_index(typeid(T))] = interfacePtr;
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

        auto begin();
        auto end();
        auto cbegin() const;
        auto cend() const;

        template <MoveableSignal T>
        void RegisterSignalHandler(ISignalHandler<T>* handler) {
            Register<ISignalHandler<T>>(handler);
        }

        template <MoveableSignal T>
        void SendSignal(T message) const {
            auto handler = Query<ISignalHandler<T>>();
            if (handler) {
                handler->Send(std::move(message));
            } else {
                OKAMI_LOG_WARNING("No signal handler registered for message type: " + std::string{typeid(T).name()});
            }
        }

    protected:
        std::unordered_map<std::type_index, std::any> m_interfaces;
    };

    struct ModuleInterface {
        MessageBus2 m_messages;
        InterfaceCollection m_interfaces;
    };

    struct Time {
		double m_deltaTime;
        double m_nextFrameTime;
		double m_lastFrameTime;
		size_t m_nextFrame;
	};

    class EngineModule {
    private:
        std::vector<std::unique_ptr<EngineModule>> m_submodules;
        bool b_started = false;
        bool b_shutdown = false;

        // Sets if children should have their ProcessFrame called automatically
        bool b_children_process_frame = true;
        bool b_children_process_startup = true;

    protected:
        inline void SetChildrenProcessFrame(bool enable) {
            b_children_process_frame = enable;
        }

        inline void SetChildrenProcessStartup(bool enable) {
            b_children_process_startup = enable;
        }

        virtual Error RegisterImpl(ModuleInterface&) = 0;
        virtual Error StartupImpl(ModuleInterface&) = 0;
        virtual void ShutdownImpl(ModuleInterface&) = 0;

        virtual Error ProcessFrameImpl(Time const&, ModuleInterface&) = 0;
        virtual Error MergeImpl(ModuleInterface&) = 0;

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

        Error Register(ModuleInterface& a);
        Error Startup(ModuleInterface& a);
        Error ProcessFrame(Time const& t, ModuleInterface& a);
        Error Merge(ModuleInterface& a);

        void Shutdown(ModuleInterface& a);

        virtual std::string GetName() const = 0;

        virtual ~EngineModule();
    };

    class EmptyModule final : public EngineModule {
    private:
        std::string m_name;
    
    public:
        inline EmptyModule(std::string name = "Empty Module") : m_name(name) {}

    protected:
        Error RegisterImpl(ModuleInterface&) override { return {};}
        Error StartupImpl(ModuleInterface&) override { return {}; }
        void ShutdownImpl(ModuleInterface&) override { }

        virtual Error ProcessFrameImpl(Time const&, ModuleInterface&) override { return {}; }
        virtual Error MergeImpl(ModuleInterface&) override { return {}; }

		std::string GetName() const override;
    };
}