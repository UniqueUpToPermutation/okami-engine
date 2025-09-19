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

        IMessageQueue<AddComponentSignal<T>>* m_addQueue = nullptr;
        IMessageQueue<UpdateComponentSignal<T>>* m_updateQueue = nullptr;
        IMessageQueue<RemoveComponentSignal<T>>* m_removeQueue = nullptr;
        IMessageQueue<EntityRemoveSignal>* m_entityRemoveQueue = nullptr;

    protected:
        Error RegisterImpl(ModuleInterface& mi) override {
            m_addQueue = mi.m_messages.CreateQueue<AddComponentSignal<T>>().get();
            m_updateQueue = mi.m_messages.CreateQueue<UpdateComponentSignal<T>>().get();
            m_removeQueue = mi.m_messages.CreateQueue<RemoveComponentSignal<T>>().get();
            m_entityRemoveQueue = mi.m_messages.CreateQueue<EntityRemoveSignal>().get();
            mi.m_interfaces.Register<IComponentView<T>>(this);
            return {};
        }

        Error StartupImpl(ModuleInterface& mi) override {
            return {};
        }

        void ShutdownImpl(ModuleInterface& mi) override {
            m_storage.clear();
            m_modified.clear();
            m_removed.clear();
            m_added.clear();
        }

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override {
            return {};
        }

        Error MergeImpl() override {
            while (auto msg = m_addQueue->GetMessage()) {
                auto& signal = *msg;
                m_storage[signal.m_entity] = signal.m_component;
                m_added.push_back(signal.m_entity);
            }

            while (auto msg = m_updateQueue->GetMessage()) {
                auto& signal = *msg;
                auto it = m_storage.find(signal.m_entity);
                if (it != m_storage.end()) {
                    it->second = signal.m_component;
                    m_modified.push_back(signal.m_entity);
                } else {
                    LogWarning("Attempted to update non-existent entity: " + std::to_string(signal.m_entity));
                }
            }

            while (auto msg = m_removeQueue->GetMessage()) {
                auto& signal = *msg;
                auto it = m_storage.find(signal.m_entity);
                if (it != m_storage.end()) {
                    m_storage.erase(it);
                    m_removed.push_back(signal.m_entity);
                } else {
                    LogWarning("Attempted to remove non-existent entity: " + std::to_string(signal.m_entity));
                }
            }

            while (auto msg = m_entityRemoveQueue->GetMessage()) {
                auto& signal = *msg;
                auto it = m_storage.find(signal.m_entity);
                if (it != m_storage.end()) {
                    m_storage.erase(it);
                    m_removed.push_back(signal.m_entity);
                }
            }

            return {};
        }

    public:
        std::string_view GetName() const override {
            return "Storage Module";
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