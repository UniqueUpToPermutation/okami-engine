#pragma once

#include "entity_manager.hpp"

#include <entt/entt.hpp>
#include <iterator>
#include <cstddef>

namespace okami {

    // Read-only view over the entity tree. Provides range-based iteration over
    // children, descendants (pre-order DFS), ancestors, and siblings of an entity.
    class EntityTreeView {
    private:
        entt::registry const* m_registry;
        EntityTreeComponent const& Get(entity_t e) const;

    public:
        explicit EntityTreeView(entt::registry const& registry);

        // ---- Direct node accessors ----

        entity_t Parent(entity_t e)      const;
        entity_t FirstChild(entity_t e)  const;
        entity_t LastChild(entity_t e)   const;
        entity_t NextSibling(entity_t e) const;
        entity_t PrevSibling(entity_t e) const;
        bool HasChildren(entity_t e) const;
        bool IsRoot(entity_t e)      const;

        // ----------------------------------------------------------------
        // Children  (direct children only)
        // ----------------------------------------------------------------

        struct ChildrenIterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type        = entity_t;
            using difference_type   = std::ptrdiff_t;
            using pointer           = const entity_t*;
            using reference         = entity_t;

            EntityTreeView const* m_view;
            entity_t m_current;

            ChildrenIterator& operator++();
            ChildrenIterator  operator++(int);
            entity_t operator*() const;
            bool operator==(ChildrenIterator const& o) const;
            bool operator!=(ChildrenIterator const& o) const;
        };

        struct ChildrenRange {
            EntityTreeView const* m_view;
            entity_t m_parent;

            ChildrenIterator begin() const;
            ChildrenIterator end()   const;
        };

        ChildrenRange Children(entity_t e) const;

        // ----------------------------------------------------------------
        // Descendants  (pre-order DFS, no heap allocation)
        // ----------------------------------------------------------------

        struct DescendantsIterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type        = entity_t;
            using difference_type   = std::ptrdiff_t;
            using pointer           = const entity_t*;
            using reference         = entity_t;

            EntityTreeView const* m_view;
            entity_t m_root;    // subtree root — never yielded, used as a sentinel
            entity_t m_current;

            DescendantsIterator& operator++();
            DescendantsIterator  operator++(int);
            entity_t operator*() const;
            bool operator==(DescendantsIterator const& o) const;
            bool operator!=(DescendantsIterator const& o) const;
        };

        struct DescendantsRange {
            EntityTreeView const* m_view;
            entity_t m_root;

            DescendantsIterator begin() const;
            DescendantsIterator end()   const;
        };

        DescendantsRange Descendants(entity_t e) const;

        // ----------------------------------------------------------------
        // Ancestors  (parent, grandparent, … up to and including the root)
        // ----------------------------------------------------------------

        struct AncestorsIterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type        = entity_t;
            using difference_type   = std::ptrdiff_t;
            using pointer           = const entity_t*;
            using reference         = entity_t;

            EntityTreeView const* m_view;
            entity_t m_current;

            AncestorsIterator& operator++();
            AncestorsIterator  operator++(int);
            entity_t operator*() const;
            bool operator==(AncestorsIterator const& o) const;
            bool operator!=(AncestorsIterator const& o) const;
        };

        struct AncestorsRange {
            EntityTreeView const* m_view;
            entity_t m_entity;

            AncestorsIterator begin() const;
            AncestorsIterator end()   const;
        };

        AncestorsRange Ancestors(entity_t e) const;

        // ----------------------------------------------------------------
        // Siblings  (all siblings, excluding the entity itself)
        // ----------------------------------------------------------------

        struct SiblingsIterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type        = entity_t;
            using difference_type   = std::ptrdiff_t;
            using pointer           = const entity_t*;
            using reference         = entity_t;

            EntityTreeView const* m_view;
            entity_t m_self;
            entity_t m_current;

            SiblingsIterator& operator++();
            SiblingsIterator  operator++(int);
            entity_t operator*() const;
            bool operator==(SiblingsIterator const& o) const;
            bool operator!=(SiblingsIterator const& o) const;
        };

        struct SiblingsRange {
            EntityTreeView const* m_view;
            entity_t m_entity;

            SiblingsIterator begin() const;
            SiblingsIterator end()   const;
        };

        SiblingsRange Siblings(entity_t e) const;
    };

} // namespace okami