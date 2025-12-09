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

        virtual Range GetModified() const = 0;
        virtual Range GetAdded() const = 0;
        virtual Range GetRemoved() const = 0;
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
        std::unordered_map<entity_t, T> m_storage;
        std::vector<entity_t> m_modified;
        std::vector<entity_t> m_removed;
        std::vector<entity_t> m_added;

    protected:
        Error RegisterImpl(InterfaceCollection& ic) override {
            ic.Register<IComponentView<T>>(this);
            return {};
        }

        Error StartupImpl(InitContext const& ic) override {
            ic.m_messages.EnsurePort<AddComponentSignal<T>>();
            ic.m_messages.EnsurePort<UpdateComponentSignal<T>>();
            ic.m_messages.EnsurePort<RemoveComponentSignal<T>>();
            ic.m_messages.EnsurePort<EntityRemoveSignal>();
            return {};
        }

        void ShutdownImpl(InitContext const& ic) override {
            m_storage.clear();
            m_modified.clear();
            m_removed.clear();
            m_added.clear();
        }

        Error ProcessFrameImpl(Time const&, ExecutionContext const& context) override {
            return {};
        }

        Error MergeImpl(MergeContext const& context) override {
            context.m_messages.Handle<AddComponentSignal<T>>([this](AddComponentSignal<T> const& signal) {
                m_storage[signal.m_entity] = signal.m_component;
                m_added.push_back(signal.m_entity);
            });

            context.m_messages.Handle<UpdateComponentSignal<T>>([this](UpdateComponentSignal<T> const& signal) {
                auto it = m_storage.find(signal.m_entity);
                if (it != m_storage.end()) {
                    it->second = signal.m_component;
                    m_modified.push_back(signal.m_entity);
                } else {
                    OKAMI_LOG_WARNING("Attempted to update non-existent entity: " + std::to_string(signal.m_entity));
                }
            });

            context.m_messages.Handle<RemoveComponentSignal<T>>([this](RemoveComponentSignal<T> const& signal) {
                auto it = m_storage.find(signal.m_entity);
                if (it != m_storage.end()) {
                    m_storage.erase(it);
                    m_removed.push_back(signal.m_entity);
                } else {
                    OKAMI_LOG_WARNING("Attempted to remove non-existent entity: " + std::to_string(signal.m_entity));
                }
            });

            context.m_messages.Handle<EntityRemoveSignal>([this](EntityRemoveSignal const& signal) {
                auto it = m_storage.find(signal.m_entity);
                if (it != m_storage.end()) {
                    m_storage.erase(it);
                    m_removed.push_back(signal.m_entity);
                }
            });
            
            return {};
        }

    public:
        std::string GetName() const override {
            auto typeName = typeid(T).name();
            return "Storage Module <" + std::string{typeName} + ">";
        }

        void ForEach(std::function<void(entity_t, T const&)> func) override {
            for (auto const& [entity, component] : m_storage) {
                func(entity, component);
            }
        }

        T const* TryGet(entity_t entity) const override {
            auto it = m_storage.find(entity);
            if (it != m_storage.end()) {
                return &it->second;
            }
            return nullptr;
        }

        IComponentView<T>::Range GetModified() const override {
            return typename IComponentView<T>::Range{m_modified.cbegin(), m_modified.cend()};
        }

        IComponentView<T>::Range GetAdded() const override {
            return typename IComponentView<T>::Range{m_added.cbegin(), m_added.cend()};
        }

        IComponentView<T>::Range GetRemoved() const override {
            return typename IComponentView<T>::Range{m_removed.cbegin(), m_removed.cend()};
        }

        bool IsEmpty() const override {
            return m_storage.empty();
        }
    };
}