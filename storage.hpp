#pragma once

#include "module.hpp"
#include "entity_manager.hpp"
#include "log.hpp"

#include <unordered_map>
#include <optional>

namespace okami {
    template <typename T>
    class IComponentView {
    public:
        struct Range {
            std::vector<entity_t>::const_iterator m_begin;
            std::vector<entity_t>::const_iterator m_end;

            auto begin() const { return m_begin; }
            auto end() const { return m_end; }
        };

        virtual ~IComponentView() = default;

        virtual void ForEach(std::function<void(entity_t, T const&)> func) = 0;
        virtual T const* TryGet(entity_t entity) const = 0;
        
        T GetOr(entity_t entity, T const& defaultValue) const {
            if (auto comp = TryGet(entity)) {
                return *comp;
            } else {
                return defaultValue;
            }
        }

        virtual bool IsEmpty() const = 0;
        
        T const& Get(entity_t entity) const {
            if (auto comp = TryGet(entity)) {
                return *comp;
            } else {
                throw std::runtime_error("Component not found for entity: " + std::to_string(entity));
            }
        }

        bool Has(entity_t entity) const {
            return TryGet(entity) != nullptr;
        }
    };

    template <typename T>
    class StorageModule final : 
        public EngineModule,
        public IComponentView<T> {
    private:
        // TODO: Remove this 
        entt::registry* m_registry = nullptr;

    protected:
        Error RegisterImpl(InterfaceCollection& ic) override {
            ic.Register<IComponentView<T>>(this);
            return {};
        }

        Error StartupImpl(InitContext const& ic) override {
            ic.m_messages.EnsurePort<AddComponentSignal<T>>();
            ic.m_messages.EnsurePort<UpdateComponentSignal<T>>();
            ic.m_messages.EnsurePort<RemoveComponentSignal<T>>();
            ic.m_messages.EnsurePort<EntityRemoveMessage>();
            ic.m_messages.EnsurePort<OnAddComponentEvent<T>>();
            ic.m_messages.EnsurePort<OnUpdateComponentEvent<T>>();
            ic.m_messages.EnsurePort<OnRemoveComponentEvent<T>>();

            m_registry = &ic.m_registry;

            return {};
        }

        Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
            if (!m_overrideAddHandler) {
                bus.Handle<AddComponentSignal<T>>([this, &params](AddComponentSignal<T> const& signal) {
                    params.m_registry.emplace<T>(signal.m_entity, signal.m_component); 
                });
            }

            if (!m_overrideUpdateHandler) {
                bus.Handle<UpdateComponentSignal<T>>([this, &params](UpdateComponentSignal<T> const& signal) {
                    params.m_registry.replace<T>(signal.m_entity, signal.m_component);
                });
            }

            if (!m_overrideRemoveHandler) {
                bus.Handle<RemoveComponentSignal<T>>([this, &params](RemoveComponentSignal<T> const& signal) {
                    params.m_registry.remove<T>(signal.m_entity);
                });
            }
            
            return {};
        }

    public:
        bool m_overrideAddHandler = false;
        bool m_overrideUpdateHandler = false;
        bool m_overrideRemoveHandler = false;

        std::string GetName() const override {
            auto typeName = typeid(T).name();
            return "Storage Module <" + std::string{typeName} + ">";
        }

        void ForEach(std::function<void(entity_t, T const&)> func) override {
            if constexpr (std::is_empty_v<T>) {
                m_registry->view<T>().each([&](auto entity) {
                    func(entity, T{});
                });
            } else {
                m_registry->view<T>().each([&](auto entity, auto& component) {
                    func(entity, component);
                });
            }
        }

        T const* TryGet(entity_t entity) const override {
            if constexpr (std::is_empty_v<T>) {
                static T empty{};
                return &empty;
            } else {
                return m_registry->try_get<T>(entity);
            }
        }

        void Set(entity_t entity, T const& component) {
            m_registry->emplace_or_replace<T>(entity, component);
        }

        bool IsEmpty() const override {
            return m_registry->view<T>().empty();
        }
    };
}