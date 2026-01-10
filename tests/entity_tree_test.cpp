#include <gtest/gtest.h>
#include "../entity_manager.hpp"
#include "../engine.hpp"

using namespace okami;
/*
class EntityTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        tree = std::make_unique<EntityTree>();
    }

    void TearDown() override {
        tree.reset();
    }

    std::unique_ptr<EntityTree> tree;
};

// Basic Entity Creation Tests
TEST_F(EntityTreeTest, CreateEntity_DefaultParent_CreatesEntityUnderRoot) {
    entity_t entity = tree->CreateEntity();
    
    EXPECT_NE(entity, kNullEntity);
    EXPECT_NE(entity, kRoot);
    EXPECT_EQ(tree->GetParent(entity), kRoot);
}

TEST_F(EntityTreeTest, CreateEntity_WithParent_CreatesEntityUnderSpecifiedParent) {
    entity_t parent = tree->CreateEntity();
    entity_t child = tree->CreateEntity(parent);
    
    EXPECT_NE(child, kNullEntity);
    EXPECT_NE(child, parent);
    EXPECT_EQ(tree->GetParent(child), parent);
}

TEST_F(EntityTreeTest, ReserveEntityId_ReturnsUniqueIds) {
    entity_t id1 = tree->ReserveEntityId();
    entity_t id2 = tree->ReserveEntityId();
    entity_t id3 = tree->ReserveEntityId();
    
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(EntityTreeTest, AddReservedEntity_WithReservedId_AddsEntitySuccessfully) {
    entity_t reservedId = tree->ReserveEntityId();
    tree->AddReservedEntity(reservedId);
    
    EXPECT_EQ(tree->GetParent(reservedId), kRoot);
}

TEST_F(EntityTreeTest, AddReservedEntity_WithParent_AddsEntityUnderParent) {
    entity_t parent = tree->CreateEntity();
    entity_t reservedId = tree->ReserveEntityId();
    tree->AddReservedEntity(reservedId, parent);
    
    EXPECT_EQ(tree->GetParent(reservedId), parent);
}

// Hierarchy Tests
TEST_F(EntityTreeTest, GetParent_RootEntity_ReturnsNullEntity) {
    EXPECT_EQ(tree->GetParent(kRoot), kNullEntity);
}

TEST_F(EntityTreeTest, SetParent_ValidEntities_ChangesParentRelationship) {
    entity_t entity = tree->CreateEntity();
    entity_t newParent = tree->CreateEntity();
    
    // Initially under root
    EXPECT_EQ(tree->GetParent(entity), kRoot);
    
    // Change parent
    tree->SetParent(entity, newParent);
    EXPECT_EQ(tree->GetParent(entity), newParent);
}

TEST_F(EntityTreeTest, SetParent_ToRoot_MovesEntityToRoot) {
    entity_t parent = tree->CreateEntity();
    entity_t child = tree->CreateEntity(parent);
    
    EXPECT_EQ(tree->GetParent(child), parent);
    
    tree->SetParent(child, kRoot);
    EXPECT_EQ(tree->GetParent(child), kRoot);
}

// Children Iteration Tests
TEST_F(EntityTreeTest, GetChildren_EntityWithNoChildren_ReturnsEmptyRange) {
    entity_t entity = tree->CreateEntity();
    auto children = tree->GetChildren(entity);
    
    EXPECT_EQ(children.begin(), children.end());
}

TEST_F(EntityTreeTest, GetChildren_EntityWithChildren_ReturnsAllChildren) {
    entity_t parent = tree->CreateEntity();
    entity_t child1 = tree->CreateEntity(parent);
    entity_t child2 = tree->CreateEntity(parent);
    entity_t child3 = tree->CreateEntity(parent);
    
    auto children = tree->GetChildren(parent);
    std::vector<entity_t> childList;
    for (auto child : children) {
        childList.push_back(child);
    }
    
    EXPECT_EQ(childList.size(), 3);
    EXPECT_TRUE(std::find(childList.begin(), childList.end(), child1) != childList.end());
    EXPECT_TRUE(std::find(childList.begin(), childList.end(), child2) != childList.end());
    EXPECT_TRUE(std::find(childList.begin(), childList.end(), child3) != childList.end());
}

TEST_F(EntityTreeTest, GetFirstChild_EntityWithChildren_ReturnsFirstChild) {
    entity_t parent = tree->CreateEntity();
    entity_t firstChild = tree->CreateEntity(parent);
    entity_t secondChild = tree->CreateEntity(parent);
    
    entity_t result = tree->GetFirstChild(parent);
    EXPECT_EQ(result, firstChild);
}

TEST_F(EntityTreeTest, GetFirstChild_EntityWithNoChildren_ReturnsNullEntity) {
    entity_t entity = tree->CreateEntity();
    
    entity_t result = tree->GetFirstChild(entity);
    EXPECT_EQ(result, kNullEntity);
}

TEST_F(EntityTreeTest, GetNextSibling_WithSiblings_ReturnsNextSibling) {
    entity_t parent = tree->CreateEntity();
    entity_t child1 = tree->CreateEntity(parent);
    entity_t child2 = tree->CreateEntity(parent);
    entity_t child3 = tree->CreateEntity(parent);
    
    entity_t nextSibling = tree->GetNextSibling(child1);
    EXPECT_TRUE(nextSibling == child2 || nextSibling == child3);
}

TEST_F(EntityTreeTest, GetNextSibling_LastChild_ReturnsNullEntity) {
    entity_t parent = tree->CreateEntity();
    entity_t child1 = tree->CreateEntity(parent);
    entity_t child2 = tree->CreateEntity(parent);
    
    // Find the last child by iterating
    entity_t lastChild = kNullEntity;
    for (auto child : tree->GetChildren(parent)) {
        lastChild = child;
    }
    
    entity_t nextSibling = tree->GetNextSibling(lastChild);
    EXPECT_EQ(nextSibling, kNullEntity);
}

// Ancestor Tests
TEST_F(EntityTreeTest, GetAncestors_DeepHierarchy_ReturnsAllAncestors) {
    entity_t level1 = tree->CreateEntity(kRoot);
    entity_t level2 = tree->CreateEntity(level1);
    entity_t level3 = tree->CreateEntity(level2);
    entity_t level4 = tree->CreateEntity(level3);
    
    auto ancestors = tree->GetAncestors(level4);
    std::vector<entity_t> ancestorList;
    for (auto ancestor : ancestors) {
        ancestorList.push_back(ancestor);
    }

    // Should contain level3, level2, level1, kRoot (in order from immediate parent to root)
    EXPECT_GE(ancestorList.size(), 3);
    EXPECT_TRUE(std::find(ancestorList.begin(), ancestorList.end(), level3) != ancestorList.end());
    EXPECT_TRUE(std::find(ancestorList.begin(), ancestorList.end(), level2) != ancestorList.end());
    EXPECT_TRUE(std::find(ancestorList.begin(), ancestorList.end(), level1) != ancestorList.end());
}

TEST_F(EntityTreeTest, GetAncestors_RootEntity_ReturnsEmptyRange) {
    auto ancestors = tree->GetAncestors(kRoot);
    EXPECT_EQ(ancestors.begin(), ancestors.end());
}

// Descendant Tests
TEST_F(EntityTreeTest, GetDescendants_EntityWithDescendants_ReturnsAllDescendants) {
    entity_t parent = tree->CreateEntity();
    entity_t child1 = tree->CreateEntity(parent);
    entity_t child2 = tree->CreateEntity(parent);
    entity_t grandchild1 = tree->CreateEntity(child1);
    entity_t grandchild2 = tree->CreateEntity(child2);
    
    auto descendants = tree->GetDescendants(parent);
    std::vector<entity_t> descendantList;
    for (auto descendant : descendants) {
        descendantList.push_back(descendant);
    }

    EXPECT_GE(descendantList.size(), 4);
    EXPECT_TRUE(std::find(descendantList.begin(), descendantList.end(), child1) != descendantList.end());
    EXPECT_TRUE(std::find(descendantList.begin(), descendantList.end(), child2) != descendantList.end());
    EXPECT_TRUE(std::find(descendantList.begin(), descendantList.end(), grandchild1) != descendantList.end());
    EXPECT_TRUE(std::find(descendantList.begin(), descendantList.end(), grandchild2) != descendantList.end());
}

TEST_F(EntityTreeTest, GetDescendants_LeafEntity_ReturnsEmptyRange) {
    entity_t leaf = tree->CreateEntity();
    auto descendants = tree->GetDescendants(leaf);
    
    EXPECT_EQ(descendants.begin(), descendants.end());
}

// Entity Removal Tests
TEST_F(EntityTreeTest, RemoveEntity_ValidEntity_RemovesEntityAndDescendants) {
    entity_t parent = tree->CreateEntity();
    entity_t child = tree->CreateEntity(parent);
    entity_t grandchild = tree->CreateEntity(child);
    
    tree->RemoveEntity(parent);
    
    // Parent and its descendants should be removed
    // We can't directly test if they're removed, but we can test that operations on them fail gracefully
    // This depends on implementation details, but we can at least ensure no crashes occur
}

TEST_F(EntityTreeTest, RemoveEntity_EntityWithSiblings_DoesNotAffectSiblings) {
    entity_t parent = tree->CreateEntity();
    entity_t child1 = tree->CreateEntity(parent);
    entity_t child2 = tree->CreateEntity(parent);
    entity_t child3 = tree->CreateEntity(parent);
    
    tree->RemoveEntity(child2);
    
    // child1 and child3 should still exist and be children of parent
    auto children = tree->GetChildren(parent);
    std::vector<entity_t> childList;
    for (auto child : children) {
        childList.push_back(child);
    }

    EXPECT_TRUE(std::find(childList.begin(), childList.end(), child1) != childList.end());
    EXPECT_TRUE(std::find(childList.begin(), childList.end(), child3) != childList.end());
    EXPECT_FALSE(std::find(childList.begin(), childList.end(), child2) != childList.end());
}

// Complex Hierarchy Tests
TEST_F(EntityTreeTest, ComplexHierarchy_MultipleOperations_MaintainsConsistency) {
    // Create a complex hierarchy
    entity_t node1 = tree->CreateEntity();
    entity_t node2 = tree->CreateEntity();
    entity_t node1_1 = tree->CreateEntity(node1);
    entity_t node1_2 = tree->CreateEntity(node1);
    entity_t node2_1 = tree->CreateEntity(node2);
    entity_t node1_1_1 = tree->CreateEntity(node1_1);
    
    // Move node1_2 under node2
    tree->SetParent(node1_2, node2);
    
    // Verify relationships
    EXPECT_EQ(tree->GetParent(node1_2), node2);
    
    auto node2Children = tree->GetChildren(node2);
    std::vector<entity_t> node2ChildList;
    for (auto child : node2Children) {
        node2ChildList.push_back(child);
    }
    EXPECT_TRUE(std::find(node2ChildList.begin(), node2ChildList.end(), node2_1) != node2ChildList.end());
    EXPECT_TRUE(std::find(node2ChildList.begin(), node2ChildList.end(), node1_2) != node2ChildList.end());
    
    auto node1Children = tree->GetChildren(node1);
    std::vector<entity_t> node1ChildList;
    for (auto child : node1Children) {
        node1ChildList.push_back(child);
    }
    EXPECT_TRUE(std::find(node1ChildList.begin(), node1ChildList.end(), node1_1) != node1ChildList.end());
    EXPECT_FALSE(std::find(node1ChildList.begin(), node1ChildList.end(), node1_2) != node1ChildList.end());
}

// Edge Cases
TEST_F(EntityTreeTest, SetParent_SelfAsParent_HandledGracefully) {
    entity_t entity = tree->CreateEntity();
    
    // This should either be a no-op or handle gracefully
    // The exact behavior depends on implementation
    tree->SetParent(entity, entity);
    
    // Entity should not be its own parent
    EXPECT_NE(tree->GetParent(entity), entity);
}

TEST_F(EntityTreeTest, MultipleReservedEntities_UniqueIds_DoNotConflict) {
    std::vector<entity_t> reservedIds;
    for (int i = 0; i < 10; ++i) {
        reservedIds.push_back(tree->ReserveEntityId());
    }
    
    // Add all reserved entities
    for (entity_t id : reservedIds) {
        tree->AddReservedEntity(id);
        EXPECT_EQ(tree->GetParent(id), kRoot);
    }
    
    // Create regular entities and ensure they don't conflict
    entity_t regularEntity = tree->CreateEntity();
    EXPECT_TRUE(std::find(reservedIds.begin(), reservedIds.end(), regularEntity) == reservedIds.end());
}
*/