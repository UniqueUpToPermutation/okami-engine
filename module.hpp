#pragma once

#include <vector>
#include <typeindex>
#include <any>
#include <queue>
#include <mutex>
#include <optional>

#include "common.hpp"
#include "log.hpp"

namespace okami {
    template <typename T>
    class IMessageSink {
    public:
        virtual void Send(T message) = 0;
        virtual ~IMessageSink() = default;
    };

    template <typename T>
    class IMessageQueue : public IMessageSink<T> {
    public:
        virtual std::optional<T> GetMessage() = 0;
        virtual ~IMessageQueue() = default;
    };

    template <typename T>
    class MessageQueue final : 
        public IMessageQueue<T>
    {
    public:
        std::optional<T> GetMessage() override {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (!m_messages.empty()) {
                T msg = std::move(m_messages.front());
                m_messages.pop();
                return msg;
            }
            return std::nullopt;
        }

        void Send(T message) override {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_messages.push(std::move(message));
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(m_mtx);
            std::queue<T> empty;
            std::swap(m_messages, empty);
        }

    private:
        std::mutex m_mtx;
        std::queue<T> m_messages;
    };

    class MessageBus final {
    public:
        template <typename T>
        std::shared_ptr<IMessageQueue<T>> CreateQueue() {
            auto queue = std::make_shared<MessageQueue<T>>();
            sinks.emplace(typeid(T), static_cast<std::shared_ptr<IMessageSink<T>>>(queue));
            return queue;
        }

        template <typename T>
        void Send(T message) {
            auto range = sinks.equal_range(typeid(T));
            for (auto it = range.first; it != range.second; ++it) {
                auto sink = std::any_cast<std::shared_ptr<IMessageSink<T>>>(it->second);
                if (sink) {
                    sink->Send(message);
                } else {

                }
            }
        }

    private:
        std::unordered_multimap<std::type_index, std::any> sinks;
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

    protected:
        std::unordered_map<std::type_index, std::any> m_interfaces;
    };

    struct ModuleInterface {
        MessageBus m_messages;
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
        virtual Error MergeImpl() = 0;

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
        Error Merge();

        void Shutdown(ModuleInterface& a);

        virtual std::string_view GetName() const = 0;

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
        virtual Error MergeImpl() override { return {}; }

		std::string_view GetName() const override;
    };
}