#include "entity_tree_view.hpp"

namespace okami {

// ---- EntityTreeView ----

EntityTreeComponent const& EntityTreeView::Get(entity_t e) const {
    return m_registry->get<EntityTreeComponent>(e);
}

EntityTreeView::EntityTreeView(entt::registry const& registry)
    : m_registry(&registry) {}

entity_t EntityTreeView::Parent(entity_t e)      const { return Get(e).m_parent; }
entity_t EntityTreeView::FirstChild(entity_t e)  const { return Get(e).m_firstChild; }
entity_t EntityTreeView::LastChild(entity_t e)   const { return Get(e).m_lastChild; }
entity_t EntityTreeView::NextSibling(entity_t e) const { return Get(e).m_nextSibling; }
entity_t EntityTreeView::PrevSibling(entity_t e) const { return Get(e).m_prevSibling; }
bool EntityTreeView::HasChildren(entity_t e) const { return Get(e).m_firstChild != kNullEntity; }
bool EntityTreeView::IsRoot(entity_t e)      const { return Get(e).m_parent == kNullEntity; }

EntityTreeView::ChildrenRange    EntityTreeView::Children(entity_t e)    const { return {this, e}; }
EntityTreeView::DescendantsRange EntityTreeView::Descendants(entity_t e) const { return {this, e}; }
EntityTreeView::AncestorsRange   EntityTreeView::Ancestors(entity_t e)   const { return {this, e}; }
EntityTreeView::SiblingsRange    EntityTreeView::Siblings(entity_t e)    const { return {this, e}; }

// ---- ChildrenIterator ----

EntityTreeView::ChildrenIterator& EntityTreeView::ChildrenIterator::operator++() {
    m_current = m_view->NextSibling(m_current);
    return *this;
}
EntityTreeView::ChildrenIterator EntityTreeView::ChildrenIterator::operator++(int) {
    auto tmp = *this; ++(*this); return tmp;
}
entity_t EntityTreeView::ChildrenIterator::operator*() const { return m_current; }
bool EntityTreeView::ChildrenIterator::operator==(ChildrenIterator const& o) const { return m_current == o.m_current; }
bool EntityTreeView::ChildrenIterator::operator!=(ChildrenIterator const& o) const { return m_current != o.m_current; }

// ---- ChildrenRange ----

EntityTreeView::ChildrenIterator EntityTreeView::ChildrenRange::begin() const {
    return {m_view, m_view->FirstChild(m_parent)};
}
EntityTreeView::ChildrenIterator EntityTreeView::ChildrenRange::end() const {
    return {m_view, kNullEntity};
}

// ---- DescendantsIterator ----

EntityTreeView::DescendantsIterator& EntityTreeView::DescendantsIterator::operator++() {
    // Pre-order DFS: go deeper first, then sideways, then up-and-sideways.
    entity_t child = m_view->FirstChild(m_current);
    if (child != kNullEntity) {
        m_current = child;
        return *this;
    }
    entity_t next = m_view->NextSibling(m_current);
    if (next != kNullEntity) {
        m_current = next;
        return *this;
    }
    entity_t p = m_view->Parent(m_current);
    while (p != m_root && p != kNullEntity) {
        next = m_view->NextSibling(p);
        if (next != kNullEntity) {
            m_current = next;
            return *this;
        }
        p = m_view->Parent(p);
    }
    m_current = kNullEntity;
    return *this;
}
EntityTreeView::DescendantsIterator EntityTreeView::DescendantsIterator::operator++(int) {
    auto tmp = *this; ++(*this); return tmp;
}
entity_t EntityTreeView::DescendantsIterator::operator*() const { return m_current; }
bool EntityTreeView::DescendantsIterator::operator==(DescendantsIterator const& o) const { return m_current == o.m_current; }
bool EntityTreeView::DescendantsIterator::operator!=(DescendantsIterator const& o) const { return m_current != o.m_current; }

// ---- DescendantsRange ----

EntityTreeView::DescendantsIterator EntityTreeView::DescendantsRange::begin() const {
    return {m_view, m_root, m_view->FirstChild(m_root)};
}
EntityTreeView::DescendantsIterator EntityTreeView::DescendantsRange::end() const {
    return {m_view, m_root, kNullEntity};
}

// ---- AncestorsIterator ----

EntityTreeView::AncestorsIterator& EntityTreeView::AncestorsIterator::operator++() {
    m_current = m_view->Parent(m_current);
    return *this;
}
EntityTreeView::AncestorsIterator EntityTreeView::AncestorsIterator::operator++(int) {
    auto tmp = *this; ++(*this); return tmp;
}
entity_t EntityTreeView::AncestorsIterator::operator*() const { return m_current; }
bool EntityTreeView::AncestorsIterator::operator==(AncestorsIterator const& o) const { return m_current == o.m_current; }
bool EntityTreeView::AncestorsIterator::operator!=(AncestorsIterator const& o) const { return m_current != o.m_current; }

// ---- AncestorsRange ----

EntityTreeView::AncestorsIterator EntityTreeView::AncestorsRange::begin() const {
    return {m_view, m_view->Parent(m_entity)};
}
EntityTreeView::AncestorsIterator EntityTreeView::AncestorsRange::end() const {
    return {m_view, kNullEntity};
}

// ---- SiblingsIterator ----

EntityTreeView::SiblingsIterator& EntityTreeView::SiblingsIterator::operator++() {
    m_current = m_view->NextSibling(m_current);
    if (m_current == m_self)
        m_current = m_view->NextSibling(m_current);
    return *this;
}
EntityTreeView::SiblingsIterator EntityTreeView::SiblingsIterator::operator++(int) {
    auto tmp = *this; ++(*this); return tmp;
}
entity_t EntityTreeView::SiblingsIterator::operator*() const { return m_current; }
bool EntityTreeView::SiblingsIterator::operator==(SiblingsIterator const& o) const { return m_current == o.m_current; }
bool EntityTreeView::SiblingsIterator::operator!=(SiblingsIterator const& o) const { return m_current != o.m_current; }

// ---- SiblingsRange ----

EntityTreeView::SiblingsIterator EntityTreeView::SiblingsRange::begin() const {
    entity_t parent = m_view->Parent(m_entity);
    if (parent == kNullEntity) return end();
    entity_t first = m_view->FirstChild(parent);
    if (first == m_entity) first = m_view->NextSibling(first);
    return {m_view, m_entity, first};
}
EntityTreeView::SiblingsIterator EntityTreeView::SiblingsRange::end() const {
    return {m_view, m_entity, kNullEntity};
}

} // namespace okami
