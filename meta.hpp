#pragma once

#include <entt/meta/meta.hpp>

#include <unordered_map>
#include <optional>

#include "module.hpp"
#include "entity_manager.hpp"

namespace okami {
    void RegisterMeta();

	// Used when you don't know the runtime type sender side, e.g. in the editor.
	struct AddComponentMetaSignal {
		entity_t m_entity = kNullEntity;
		entt::meta_any m_component;
	};

    // Used when you don't know the runtime type sender side, e.g. in the editor.
	struct UpdateComponentMetaSignal {
		entity_t m_entity = kNullEntity;
		entt::meta_any m_component;
	};

    struct RemoveComponentMetaSignal {
        entity_t m_entity = kNullEntity;
		entt::meta_type m_type;
	};

	struct AddCtxMetaSignal {
		entt::meta_any m_value;
	};

    struct UpdateCtxMetaSignal {
		entt::meta_any m_value;
	};

    struct RemoveCtxMetaSignal {
		entt::meta_type m_type;
	};

    struct ComponentMetaData {
        bool b_defaultAddHandler = true;
        bool b_defaultRemoveHandler = true;
        bool b_defaultUpdateHandler = true;
        bool b_allowMetaConversion = true; // Whether this component can be automatically converted to/from meta_any for editor messages
        
        bool b_showInEditor = true; // Whether this component should be shown in the editor's context window
        std::string_view m_displayName; // Optional human-readable name for editor display

        inline bool NeedsDefaultHandler() const {
            return b_defaultAddHandler || 
                b_defaultRemoveHandler || 
                b_defaultUpdateHandler ||
                b_allowMetaConversion;
        }
    };

    struct CtxMetaData {
        bool b_defaultAddHandler = true;
        bool b_defaultRemoveHandler = true;
        bool b_defaultUpdateHandler = true;
        bool b_allowMetaConversion = true; // Whether this ctx can be automatically converted to/from meta_any for editor messages
        
        bool b_showInEditor = true; // Whether this ctx variable should be shown in the editor's context window
        std::string_view m_displayName; // Optional human-readable name for editor display
        
        inline bool NeedsDefaultHandler() const {
            return b_defaultAddHandler || 
                b_defaultRemoveHandler || 
                b_defaultUpdateHandler || 
                b_allowMetaConversion;
        }
    };

    struct MetaData {
        std::optional<ComponentMetaData> m_componentMetaData;
        std::optional<CtxMetaData>       m_ctxMetaData;

        inline bool IsComponent() const {
            return m_componentMetaData.has_value();
        }

        inline bool IsCtx() const {
            return m_ctxMetaData.has_value();
        }

        inline static MetaData ComponentDefault() {
            MetaData metaData;
            metaData.m_componentMetaData = ComponentMetaData();
            return metaData;
        }

        inline static MetaData CtxDefault() {
            MetaData metaData;
            metaData.m_ctxMetaData = CtxMetaData();
            return metaData;
        }
    };

    struct MetaType {
        entt::meta_type m_type;
        MetaData const& GetMetaData() const {
            return static_cast<MetaData const&>(m_type.custom());
        }
    };

    // Attached to individual data fields via meta_factory::data().custom<FieldMeta>().
    // Provides a human-readable display name for the inspector.
    struct FieldMeta {
        const char* m_displayName = nullptr;
        bool b_showInEditor = true; // Whether this field should be shown in the editor's inspector when its parent component/ctx is selected
    };

    // Function pointer type used to retrieve a ctx variable as meta_any.
    using CtxGetterFn = entt::meta_any(*)(entt::registry const&);

    std::span<const MetaType> GetRegisteredComponents();
    std::span<const MetaType> GetRegisteredCtxs();
    // Returns the ctx value as a meta_any (wrapping a reference) if the ctx
    // variable is present in registry, or an empty meta_any otherwise.
    entt::meta_any GetCtxValue(entt::registry const&, entt::meta_type);
    

    /*
        Implements default
    */
    struct MetaDataModuleFactory {
        std::unique_ptr<EngineModule> operator()();
    };
}