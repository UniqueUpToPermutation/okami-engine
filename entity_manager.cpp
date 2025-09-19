#include "entity_manager.hpp"
#include <unordered_map>
#include <iostream>

#include "common.hpp"
#include "pool.hpp"

using namespace okami;

// Define WorldNode and WorldImpl in the .cpp file to keep them private
struct EntityTreeNode {
	entity_t m_entityId = kNullEntity;
	entity_t m_parent = kNullEntity;
	entity_t m_firstChild = kNullEntity;
	entity_t m_lastChild = kNullEntity;
	entity_t m_nextSibling = kNullEntity;
	entity_t m_previousSibling = kNullEntity;

	EntityTreeNode(entity_t id) : m_entityId(id) {}
	EntityTreeNode() = default; // Default constructor for empty nodes
};

namespace okami {
	struct EntityTreeImpl {
        std::atomic<entity_t> m_nextEntityId{1}; // Start from 1 since 0 is root
		std::unordered_map<entity_t, EntityTreeNode> m_entityMap;
	};
}

void EntityTreeImplDeleter::operator()(EntityTreeImpl* impl) const {
	delete impl;
}

EntityTree::EntityTree() : m_impl(std::unique_ptr<EntityTreeImpl, EntityTreeImplDeleter>(new EntityTreeImpl())) {
	m_impl->m_entityMap[kRoot] = EntityTreeNode(kRoot);
}

entity_t EntityTree::CreateEntity(entity_t parent) {
	auto id = ReserveEntityId();
	AddReservedEntity(id, parent);
	return id;
}

void EntityTree::AddReservedEntity(entity_t entity, entity_t parent) {
    OKAMI_ASSERT(!m_impl->m_entityMap.contains(entity), "Entity ID already exists in the tree");

	EntityTreeNode newNode(entity);
	newNode.m_parent = parent;

	auto& parentNode = m_impl->m_entityMap[parent];
	if (parentNode.m_firstChild == kNullEntity) {
		parentNode.m_firstChild = entity;
		parentNode.m_lastChild = entity;
	}
	else {
		// Link to the last child
		if (parentNode.m_lastChild != kNullEntity) {
			newNode.m_previousSibling = parentNode.m_lastChild;
			m_impl->m_entityMap[parentNode.m_lastChild].m_nextSibling = entity;
			parentNode.m_lastChild = entity;
		}
	}

	// Update the map with the new node
	m_impl->m_entityMap[entity] = newNode;
}

void EntityTree::RemoveEntity(entity_t entity) {
	if (entity == kRoot) {
		throw std::runtime_error("Cannot remove root entity");
	}

	OKAMI_ASSERT(m_impl->m_entityMap.contains(entity), "Entity must exist in the tree");
	const auto& node = m_impl->m_entityMap[entity];

	// Remove children recursively
	entity_t child = node.m_firstChild;
	while (child != kNullEntity) {
		auto nextChild = m_impl->m_entityMap[child].m_nextSibling;
		RemoveEntity(child);
		child = nextChild;
	}

	// Update parent's child pointers
	if (node.m_parent != kNullEntity) {
		auto& parent = m_impl->m_entityMap[node.m_parent];
		if (parent.m_firstChild == entity) {
			parent.m_firstChild = node.m_nextSibling;
		}
		if (parent.m_lastChild == entity) {
			parent.m_lastChild = node.m_previousSibling;
		}
	}

	// Update sibling links
	if (node.m_previousSibling != kNullEntity) {
		m_impl->m_entityMap[node.m_previousSibling].m_nextSibling = node.m_nextSibling;
	}
	if (node.m_nextSibling != kNullEntity) {
		m_impl->m_entityMap[node.m_nextSibling].m_previousSibling = node.m_previousSibling;
	}

	m_impl->m_entityMap.erase(entity);
}

void EntityTree::SetParent(entity_t entity, entity_t newParent) {
	// Cannot reparent the root entity
	if (entity == kRoot) {
		throw std::runtime_error("Cannot reparent root entity");
	}

	// Validate entities exist
    OKAMI_ASSERT(m_impl->m_entityMap.contains(entity), "Entity must exist in the tree");
    OKAMI_ASSERT(m_impl->m_entityMap.contains(newParent), "New parent entity must exist in the tree");

	entity_t oldParent = m_impl->m_entityMap[entity].m_parent;

	// If already the parent, do nothing
	if (oldParent == newParent) {
		return;
	}

	// Check for circular dependency - newParent cannot be a descendant of entity
	entity_t checkParent = newParent;
	while (checkParent != kNullEntity) {
		if (checkParent == entity) {
			// Circular dependency detected, do nothing
			return;
		}
		checkParent = m_impl->m_entityMap[checkParent].m_parent;
	}

	// Remove entity from old parent's child list
	auto& entityNode = m_impl->m_entityMap[entity];
	if (oldParent != kNullEntity) {
		auto& oldParentNode = m_impl->m_entityMap[oldParent];

		// Update parent's first/last child pointers
		if (oldParentNode.m_firstChild == entity) {
			oldParentNode.m_firstChild = entityNode.m_nextSibling;
		}
		if (oldParentNode.m_lastChild == entity) {
			oldParentNode.m_lastChild = entityNode.m_previousSibling;
		}
	}

	// Update sibling links in old parent
	if (entityNode.m_previousSibling != kNullEntity) {
		m_impl->m_entityMap[entityNode.m_previousSibling].m_nextSibling = entityNode.m_nextSibling;
	}
	if (entityNode.m_nextSibling != kNullEntity) {
		m_impl->m_entityMap[entityNode.m_nextSibling].m_previousSibling = entityNode.m_previousSibling;
	}

	// Clear entity's sibling links
	entityNode.m_nextSibling = kNullEntity;
	entityNode.m_previousSibling = kNullEntity;

	// Set new parent
	entityNode.m_parent = newParent;

	// Add entity to new parent's child list
	auto& newParentNode = m_impl->m_entityMap[newParent];
	if (newParentNode.m_firstChild == kNullEntity) {
		// First child
		newParentNode.m_firstChild = entity;
		newParentNode.m_lastChild = entity;
	} else {
		// Add as last child
		if (newParentNode.m_lastChild != kNullEntity) {
			entityNode.m_previousSibling = newParentNode.m_lastChild;
			m_impl->m_entityMap[newParentNode.m_lastChild].m_nextSibling = entity;
		}
		newParentNode.m_lastChild = entity;
	}
}

entity_t EntityTree::GetParent(entity_t entity) const {
	OKAMI_ASSERT(m_impl != nullptr, "EntityTreeImpl must be initialized");
	return m_impl->m_entityMap[entity].m_parent;
}

entity_t EntityTree::GetNextSibling(entity_t entity) const {
	OKAMI_ASSERT(m_impl != nullptr, "EntityTreeImpl must be initialized");
	return m_impl->m_entityMap[entity].m_nextSibling;
}

entity_t EntityTree::GetFirstChild(entity_t entity) const {
	OKAMI_ASSERT(m_impl != nullptr, "EntityTreeImpl must be initialized");
	return m_impl->m_entityMap[entity].m_firstChild;
}

// Iterator range implementations
EntityIteratorRange<EntityChildrenIterator> EntityTree::GetChildren(entity_t entity) const {
	entity_t firstChild = GetFirstChild(entity);
	return EntityIteratorRange<EntityChildrenIterator>(
		EntityChildrenIterator(this, firstChild),
		EntityChildrenIterator(this, kNullEntity)
	);
}

EntityIteratorRange<EntityAncestorIterator> EntityTree::GetAncestors(entity_t entity) const {
	entity_t parent = GetParent(entity);
	return EntityIteratorRange<EntityAncestorIterator>(
		EntityAncestorIterator(this, parent),
		EntityAncestorIterator(this, kNullEntity)
	);
}

EntityIteratorRange<EntityPrefixIterator> EntityTree::GetDescendants(entity_t entity) const {
	entity_t firstChild = GetFirstChild(entity);
	return EntityIteratorRange<EntityPrefixIterator>(
		EntityPrefixIterator(this, firstChild, entity),
		EntityPrefixIterator(this, kNullEntity, entity)
	);
}

// Iterator implementations
EntityChildrenIterator& EntityChildrenIterator::operator++() {
	if (m_current != kNullEntity) {
		m_current = m_world->GetNextSibling(m_current);
	}
	return *this;
}

EntityAncestorIterator& EntityAncestorIterator::operator++() {
	if (m_current != kNullEntity) {
		m_current = m_world->GetParent(m_current);
	}
	return *this;
}

EntityPrefixIterator& EntityPrefixIterator::operator++() {
	if (m_current == kNullEntity) {
		return *this;
	}

	// Try to go to first child
	entity_t firstChild = m_world->GetFirstChild(m_current);
	if (firstChild != kNullEntity) {
		m_current = firstChild;
		return *this;
	}

	// No children, try next sibling
	entity_t nextSibling = m_world->GetNextSibling(m_current);
	if (nextSibling != kNullEntity) {
		m_current = nextSibling;
		return *this;
	}

	// No sibling, go up the tree
	entity_t current = m_current;
	while (current != kNullEntity && current != m_root) {
		entity_t parent = m_world->GetParent(current);
		if (parent == kNullEntity || parent == m_root) {
			break;
		}

		entity_t parentSibling = m_world->GetNextSibling(parent);
		if (parentSibling != kNullEntity) {
			m_current = parentSibling;
			return *this;
		}

		current = parent;
	}

	m_current = kNullEntity;
	return *this;
}

entity_t EntityTree::ReserveEntityId() {
    return m_impl->m_nextEntityId.fetch_add(1);
}

struct EntityCreate {
    entity_t m_entity;
    entity_t m_parent;
};

struct EntityRemove {
    entity_t m_entity;
};

struct EntityParentChange {
    entity_t m_entity;
    entity_t m_oldParent;
    entity_t m_newParent;
};

class EntityManager : public EngineModule, public IEntityManager {
private:
    EntityTree m_tree;
    std::queue<EntityCreate> m_createQueue;
    std::mutex m_createMutex;
    std::queue<EntityRemove> m_removeQueue;
    std::mutex m_removeMutex;
    std::queue<EntityParentChange> m_parentChangeQueue;
    std::mutex m_parentChangeMutex;

protected:
    Error RegisterImpl(ModuleInterface& a) override {
        a.m_interfaces.Register<IEntityManager>(this);
        return {};
    }
    
    Error StartupImpl(ModuleInterface&) override { return {}; }
    void ShutdownImpl(ModuleInterface&) override { }

    Error ProcessFrameImpl(Time const& t, ModuleInterface& a) override { return {}; }
    Error MergeImpl() override { 
        {
            std::lock_guard lock(m_createMutex);
            while (!m_createQueue.empty()) {
                auto& req = m_createQueue.front();
                m_tree.AddReservedEntity(req.m_entity, req.m_parent);
                m_createQueue.pop();
            }
        }

        {
            std::lock_guard lock(m_removeMutex);
            while (!m_removeQueue.empty()) {
                auto& req = m_removeQueue.front();
                m_tree.RemoveEntity(req.m_entity);
                m_removeQueue.pop();
            }
        }

        {
            std::lock_guard lock(m_parentChangeMutex);
            while (!m_parentChangeQueue.empty()) {
                auto& req = m_parentChangeQueue.front();
                m_tree.SetParent(req.m_entity, req.m_newParent);
                m_parentChangeQueue.pop();
            }
        }

        return {};
    }

    std::string_view GetName() const override {
        return "Entity Manager";
    }

public:
    EntityTree const& GetTree() const override {
        return m_tree;
    }

    entity_t CreateEntity(entity_t parent = kRoot) override {
        entity_t newEntity = m_tree.ReserveEntityId();
        std::lock_guard lock(m_createMutex);
        m_createQueue.push({newEntity, parent});
        return newEntity;
    }

    void RemoveEntity(entity_t entity) override {
        std::lock_guard lock(m_removeMutex);
        m_removeQueue.push({entity});
    }

    void SetParent(entity_t entity, entity_t parent = kRoot) override { 
        entity_t oldParent = m_tree.GetParent(entity);
        std::lock_guard lock(m_parentChangeMutex);
        m_parentChangeQueue.push({entity, oldParent, parent});
    }
};

std::unique_ptr<EngineModule> EntityManagerFactory::operator()() {
    return std::make_unique<EntityManager>();
}