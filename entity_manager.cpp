#include "entity_manager.hpp"
#include <unordered_map>
#include <iostream>

#include "common.hpp"
#include "pool.hpp"

#include <entt/entt.hpp>

using namespace okami;

class EntityManager : public EngineModule, public IEntityManager {
private:
    entt::registry& m_registry;

protected:
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        interfaces.Register<IEntityManager>(this);
        return {};
    }
    
    Error StartupImpl(InitContext const& a) override {
        auto root = a.m_registry.create();
		a.m_registry.emplace<EntityTreeComponent>(root);
        a.m_registry.emplace<NameComponent>(root, "Root");

        a.m_registry.ctx().emplace<EntityManagerCtx>(
            EntityManagerCtx{ .m_rootEntity = root }); 
            
		a.m_messages.EnsurePort<EntityRemoveMessage>();
		a.m_messages.EnsurePort<EntityParentChangeSignal>();
        a.m_messages.EnsurePort<EntityCreatedSignal>();

		return {};
	}
    void ShutdownImpl(InitContext const&) override { }

    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        auto& reg = params.m_registry;
        auto root = reg.ctx().get<EntityManagerCtx>().m_rootEntity;

        // Removes an entity from its current parent's child linked-list.
        auto detachFromParent = [&reg](entity_t entity) {
            auto& tree = reg.get<EntityTreeComponent>(entity);
            if (tree.m_parent == kNullEntity) return;

            auto& parentTree = reg.get<EntityTreeComponent>(tree.m_parent);

            if (tree.m_prevSibling != kNullEntity)
                reg.get<EntityTreeComponent>(tree.m_prevSibling).m_nextSibling = tree.m_nextSibling;
            else
                parentTree.m_firstChild = tree.m_nextSibling;

            if (tree.m_nextSibling != kNullEntity)
                reg.get<EntityTreeComponent>(tree.m_nextSibling).m_prevSibling = tree.m_prevSibling;
            else
                parentTree.m_lastChild = tree.m_prevSibling;

            tree.m_parent = kNullEntity;
            tree.m_prevSibling = kNullEntity;
            tree.m_nextSibling = kNullEntity;
        };

        // Appends an entity as the last child of the given parent.
        auto attachToParent = [&reg](entity_t entity, entity_t parent) {
            auto& tree = reg.get<EntityTreeComponent>(entity);
            auto& parentTree = reg.get<EntityTreeComponent>(parent);

            tree.m_parent = parent;
            tree.m_prevSibling = parentTree.m_lastChild;
            tree.m_nextSibling = kNullEntity;

            if (parentTree.m_lastChild != kNullEntity)
                reg.get<EntityTreeComponent>(parentTree.m_lastChild).m_nextSibling = entity;
            else
                parentTree.m_firstChild = entity;

            parentTree.m_lastChild = entity;
        };

        bus.Handle<EntityCreatedSignal>([this](EntityCreatedSignal const& msg) {
            m_registry.emplace<EntityTreeComponent>(msg.m_entity);
        });
        
        bus.Handle<EntityParentChangeSignal>([&reg, root, &detachFromParent, &attachToParent](EntityParentChangeSignal const& msg) {
            detachFromParent(msg.m_entity);

            auto newParent = msg.m_newParent == kNullEntity ? root : msg.m_newParent;
            attachToParent(msg.m_entity, newParent);
        });

        bus.Handle<EntityRemoveMessage>([this, &reg, &detachFromParent](EntityRemoveMessage const& msg) {
            // Iterative post-order destruction: collect all descendants, then destroy bottom-up.
            std::vector<entity_t> toDestroy;
            std::vector<entity_t> stack = {msg.m_entity};
            while (!stack.empty()) {
                entity_t current = stack.back();
                stack.pop_back();
                toDestroy.push_back(current);
                entity_t child = reg.get<EntityTreeComponent>(current).m_firstChild;
                while (child != kNullEntity) {
                    stack.push_back(child);
                    child = reg.get<EntityTreeComponent>(child).m_nextSibling;
                }
            }
            // Detach the root entity from its parent first to keep the rest of the tree intact.
            detachFromParent(msg.m_entity);
            // Destroy all collected entities (children first doesn't matter; tree links are no longer needed).
            for (entity_t e : toDestroy)
                m_registry.destroy(e);
        });

        return {};
    }

    std::string GetName() const override {
        return "Entity Manager";
    }

public:
	EntityManager(entt::registry& registry) : m_registry(registry) {}

    entity_t CreateEntity() override {
		static_assert(ENTT_USE_ATOMIC, "EntityManager requires ENTT_USE_ATOMIC to be defined for thread-safe entity creation");
        return m_registry.create(); // Assumed atmoic
    }
};

std::unique_ptr<EngineModule> EntityManagerFactory::operator()(entt::registry& registry) {
    return std::make_unique<EntityManager>(registry);
}