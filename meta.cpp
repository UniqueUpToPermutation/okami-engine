#include "meta.hpp"

#include <entt/meta/factory.hpp>
#include <entt/meta/resolve.hpp>

#include "camera.hpp"
#include "transform.hpp"
#include "light.hpp"
#include "sky.hpp"
#include "renderer.hpp"
#include "camera_controllers.hpp"
#include "entity_manager.hpp"
#include "entity_tree_view.hpp"
#include "animation.hpp"
#include "editor.hpp"

using namespace okami;
using namespace entt::literals;

bool g_built = false;

template <typename ComponentT>
class DefaultMergeModule final : public EngineModule {
protected:
    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        auto meta = static_cast<MetaData const&>(entt::resolve<ComponentT>().custom());

        if (meta.IsComponent()) {
            auto& registry = params.m_registry;

            if (meta.m_componentMetaData->b_defaultAddHandler) {
                bus.Handle<AddComponentSignal<ComponentT>>([&](AddComponentSignal<ComponentT> const& signal) {
                    registry.emplace<ComponentT>(signal.m_entity, signal.m_component);
                });
            }

            if (meta.m_componentMetaData->b_defaultUpdateHandler) {
                bus.Handle<UpdateComponentSignal<ComponentT>>([&](UpdateComponentSignal<ComponentT> const& signal) {
                    registry.replace<ComponentT>(signal.m_entity, signal.m_component);
                });
            }

            if (meta.m_componentMetaData->b_defaultRemoveHandler) {
                bus.Handle<RemoveComponentSignal<ComponentT>>([&](RemoveComponentSignal<ComponentT> const& signal) {
                    registry.remove<ComponentT>(signal.m_entity);
                });
            }
        }

        return {};
    }

    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
        auto meta = static_cast<MetaData const&>(entt::resolve<ComponentT>().custom());

        if (meta.IsComponent() && meta.m_componentMetaData->b_allowMetaConversion) {
            // Convert meta add component signals of this type to real add component signals
            graph.AddMessageNode([](
                JobContext&, 
                In<AddComponentMetaSignal> addComponent, 
                Out<AddComponentSignal<ComponentT>> addComponentOut) -> Error {
                
                addComponent.Read([&](std::span<AddComponentMetaSignal const> signals) {
                    const auto targetId = entt::resolve<ComponentT>().id();
                    auto begin = std::lower_bound(signals.begin(), signals.end(), targetId, [](AddComponentMetaSignal const& signal, entt::id_type typeId) {
                        return signal.m_component.type().id() < typeId;
                    });
                    auto end = std::upper_bound(begin, signals.end(), targetId, [](entt::id_type typeId, AddComponentMetaSignal const& signal) {
                        return typeId < signal.m_component.type().id();
                    });
                    for (auto it = begin; it != end; ++it) {
                        if (it->m_component.type() == entt::resolve<ComponentT>()) {
                            addComponentOut.Send(AddComponentSignal<ComponentT>{it->m_entity, it->m_component.template cast<ComponentT>()});
                        }
                    }
                });

                return {};
            });

            // Convert meta update component signals of this type to real update component signals
            graph.AddMessageNode([](
                JobContext&, 
                In<UpdateComponentMetaSignal> addComponent, 
                Out<UpdateComponentSignal<ComponentT>> addComponentOut) -> Error {
                
                addComponent.Read([&](std::span<UpdateComponentMetaSignal const> signals) {
                    const auto targetId = entt::resolve<ComponentT>().id();
                    auto begin = std::lower_bound(signals.begin(), signals.end(), targetId, [](UpdateComponentMetaSignal const& signal, entt::id_type typeId) {
                        return signal.m_component.type().id() < typeId;
                    });
                    auto end = std::upper_bound(begin, signals.end(), targetId, [](entt::id_type typeId, UpdateComponentMetaSignal const& signal) {
                        return typeId < signal.m_component.type().id();
                    });
                    for (auto it = begin; it != end; ++it) {
                        if (it->m_component.type() == entt::resolve<ComponentT>()) {
                            addComponentOut.Send(UpdateComponentSignal<ComponentT>{it->m_entity, it->m_component.template cast<ComponentT>()});
                        }
                    }
                });

                return {};
            });

            // Convert meta remove component signals of this type to real remove component signals
            graph.AddMessageNode([](
                JobContext&, 
                In<RemoveComponentMetaSignal> removeComponent, 
                Out<RemoveComponentSignal<ComponentT>> removeComponentOut) -> Error {
                
                removeComponent.Read([&](std::span<RemoveComponentMetaSignal const> signals) {
                    const auto targetId = entt::resolve<ComponentT>().id();
                    auto begin = std::lower_bound(signals.begin(), signals.end(), targetId, [](RemoveComponentMetaSignal const& signal, entt::id_type typeId) {
                        return signal.m_type.id() < typeId;
                    });
                    auto end = std::upper_bound(begin, signals.end(), targetId, [](entt::id_type typeId, RemoveComponentMetaSignal const& signal) {
                        return typeId < signal.m_type.id();
                    });
                    for (auto it = begin; it != end; ++it) {
                        removeComponentOut.Send(RemoveComponentSignal<ComponentT>{it->m_entity});
                    }
                });

                return {};
            });
        }

        return {};
    }

    std::string GetName() const override {
        return "MergeModule (Default for " + std::string(entt::resolve<ComponentT>().info().name()) + ")";
    }
};

template <typename T>
class DefaultCtxMergeModule final : public EngineModule {
protected:
    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        auto& registry = params.m_registry;

        auto meta = static_cast<MetaData const&>(entt::resolve<T>().custom());

        if (meta.m_ctxMetaData->b_defaultAddHandler) {
            bus.Handle<AddCtxSignal<T>>([&](AddCtxSignal<T> const& signal) {
                if (!registry.ctx().contains<T>()) {
                    registry.ctx().emplace<T>(signal.m_value);
                }
            });
        }

        if (meta.m_ctxMetaData->b_defaultUpdateHandler) {
            bus.Handle<UpdateCtxSignal<T>>([&](UpdateCtxSignal<T> const& signal) {
                registry.ctx().erase<T>();
                registry.ctx().emplace<T>(signal.m_value);
            });
        }

        if (meta.m_ctxMetaData->b_defaultRemoveHandler) {
            bus.Handle<RemoveCtxSignal<T>>([&](RemoveCtxSignal<T> const&) {
                registry.ctx().erase<T>();
            });
        }

        return {};
    }

    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
        auto meta = static_cast<MetaData const&>(entt::resolve<T>().custom());

        if (meta.IsCtx() && meta.m_ctxMetaData->b_allowMetaConversion) {
           // Convert meta add ctx signals of this type to real add ctx signals
            graph.AddMessageNode([](
                JobContext&, 
                In<AddCtxMetaSignal> addCtx, 
                Out<AddCtxSignal<T>> addCtxOut) -> Error {
                
                addCtx.Read([&](std::span<AddCtxMetaSignal const> signals) {
                    const auto targetId = entt::resolve<T>().id();
                    auto begin = std::lower_bound(signals.begin(), signals.end(), targetId, [](AddCtxMetaSignal const& signal, entt::id_type typeId) {
                        return signal.m_value.type().id() < typeId;
                    });
                    auto end = std::upper_bound(begin, signals.end(), targetId, [](entt::id_type typeId, AddCtxMetaSignal const& signal) {
                        return typeId < signal.m_value.type().id();
                    });
                    for (auto it = begin; it != end; ++it) {
                        if (it->m_value.type() == entt::resolve<T>()) {
                            addCtxOut.Send(AddCtxSignal<T>{it->m_value.template cast<T>()});
                        }
                    }
                });

                return {};
            });

            // Convert meta update ctx signals of this type to real update ctx signals
            graph.AddMessageNode([](
                JobContext&, 
                In<UpdateCtxMetaSignal> updateCtx, 
                Out<UpdateCtxSignal<T>> updateCtxOut) -> Error {
                
                updateCtx.Read([&](std::span<UpdateCtxMetaSignal const> signals) {
                    const auto targetId = entt::resolve<T>().id();
                    auto begin = std::lower_bound(signals.begin(), signals.end(), targetId, [](UpdateCtxMetaSignal const& signal, entt::id_type typeId) {
                        return signal.m_value.type().id() < typeId;
                    });
                    auto end = std::upper_bound(begin, signals.end(), targetId, [](entt::id_type typeId, UpdateCtxMetaSignal const& signal) {
                        return typeId < signal.m_value.type().id();
                    });
                    for (auto it = begin; it != end; ++it) {
                        if (it->m_value.type() == entt::resolve<T>()) {
                            updateCtxOut.Send(UpdateCtxSignal<T>{it->m_value.template cast<T>()});
                        }
                    }
                });

                return {};
            });

            // Convert meta remove ctx signals of this type to real remove ctx signals
            graph.AddMessageNode([](
                JobContext&, 
                In<RemoveCtxMetaSignal> removeCtx, 
                Out<RemoveCtxSignal<T>> removeCtxOut) -> Error {
                    removeCtx.Read([&](std::span<RemoveCtxMetaSignal const> signals) {
                        const auto targetId = entt::resolve<T>().id();
                        auto begin = std::lower_bound(signals.begin(), signals.end(), targetId, [](RemoveCtxMetaSignal const& signal, entt::id_type typeId) {
                            return signal.m_type.id() < typeId;
                        });
                        auto end = std::upper_bound(begin, signals.end(), targetId, [](entt::id_type typeId, RemoveCtxMetaSignal const& signal) {
                            return typeId < signal.m_type.id();
                        });
                        for (auto it = begin; it != end; ++it) {
                            removeCtxOut.Send(RemoveCtxSignal<T>{});
                        }
                    });

                    return {};
                });
        }

        return {};
    }

    std::string GetName() const override {
        return "CtxMergeModule (Default for " + std::string(entt::resolve<T>().info().name()) + ")";
    }
};


std::vector<MetaType> g_registeredComponents;
std::vector<MetaType> g_registeredCtxs;
static std::unordered_map<entt::id_type, okami::CtxGetterFn> g_ctxGetters;

std::span<const MetaType> okami::GetRegisteredComponents() {
    return g_registeredComponents;
}

std::span<const MetaType> okami::GetRegisteredCtxs() {
    return g_registeredCtxs;
}

entt::meta_any okami::GetCtxValue(entt::registry const& registry, entt::meta_type type) {
    auto it = g_ctxGetters.find(type.id());
    if (it != g_ctxGetters.end()) return it->second(registry);
    return {};
}

class MetaDataModule final : public EngineModule {
protected:
    template <typename ComponentT>
    auto RegisterComponent(entt::hashed_string name, MetaData metaData = MetaData::ComponentDefault()) {
        if (metaData.IsComponent()) {
            if (metaData.m_componentMetaData->NeedsDefaultHandler()) {
                CreateChild<DefaultMergeModule<ComponentT>>();
            }
        }
        
        auto factory = entt::meta_factory<ComponentT>().custom<MetaData>(std::move(metaData));
        g_registeredComponents.push_back(MetaType{entt::resolve<ComponentT>()});
        return factory;
    }

    template <typename T>
    auto RegisterCtx(entt::hashed_string name, MetaData metaData = MetaData::CtxDefault()) {
        if (metaData.IsCtx()) {
            if (metaData.m_ctxMetaData->NeedsDefaultHandler()) {
                CreateChild<DefaultCtxMergeModule<T>>();
            }
        }

        g_ctxGetters[entt::resolve<T>().id()] =
            +[](entt::registry const& r) -> entt::meta_any {
                if (auto* p = r.ctx().find<T>())
                    return entt::resolve<T>().from_void(const_cast<T*>(p));
                return {};
            };

        auto factory = entt::meta_factory<T>().custom<MetaData>(std::move(metaData));
        g_registeredCtxs.push_back(MetaType{entt::resolve<T>()});
        return factory;
    }

    void Build() {
        entt::meta_reset();
        
        g_registeredComponents.clear();
        g_registeredCtxs.clear();
        g_ctxGetters.clear();

        entt::meta_factory<glm::vec3>()
            .type("vec3"_hs)
            .data<&glm::vec3::x>("x"_hs).custom<FieldMeta>(FieldMeta{"x"})
            .data<&glm::vec3::y>("y"_hs).custom<FieldMeta>(FieldMeta{"y"})
            .data<&glm::vec3::z>("z"_hs).custom<FieldMeta>(FieldMeta{"z"});
        entt::meta_factory<glm::vec4>()
            .type("vec4"_hs)
            .data<&glm::vec4::x>("x"_hs).custom<FieldMeta>(FieldMeta{"x"})
            .data<&glm::vec4::y>("y"_hs).custom<FieldMeta>(FieldMeta{"y"})
            .data<&glm::vec4::z>("z"_hs).custom<FieldMeta>(FieldMeta{"z"})
            .data<&glm::vec4::w>("w"_hs).custom<FieldMeta>(FieldMeta{"w"});
        entt::meta_factory<glm::quat>()
            .type("quat"_hs)
            .data<&glm::quat::x>("x"_hs).custom<FieldMeta>(FieldMeta{"x"})
            .data<&glm::quat::y>("y"_hs).custom<FieldMeta>(FieldMeta{"y"})
            .data<&glm::quat::z>("z"_hs).custom<FieldMeta>(FieldMeta{"z"})
            .data<&glm::quat::w>("w"_hs).custom<FieldMeta>(FieldMeta{"w"});
        entt::meta_factory<glm::mat3>()
            .type("mat3"_hs);

        RegisterComponent<Camera>("Camera"_hs);

        RegisterComponent<Transform>("Transform"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Transform",
                .b_writeable = true,
            }
        });

        RegisterComponent<EntityTreeComponent>("EntityTree"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Entity Tree",
                .b_showInEditor = false,
            }
        });

        RegisterComponent<AmbientLightComponent>("AmbientLight"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Ambient Light",
                .b_writeable = true,
            }
        });

        entt::meta_factory<AmbientLightComponent>()
            .data<&AmbientLightComponent::m_color>("color"_hs).custom<FieldMeta>(FieldMeta{
                .m_displayName = "Color",
                .m_hint = FieldHint::Color})
            .data<&AmbientLightComponent::m_intensity>("intensity"_hs).custom<FieldMeta>(FieldMeta{
                .m_displayName = "Intensity"});

        RegisterComponent<DirectionalLightComponent>("DirectionalLight"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Directional Light",
                .b_writeable = true,
            }
        });
        
        entt::meta_factory<DirectionalLightComponent>()
            .data<&DirectionalLightComponent::m_direction>("direction"_hs).custom<FieldMeta>(FieldMeta{
                .m_displayName = "Direction",
                .m_hint = FieldHint::Direction})
            .data<&DirectionalLightComponent::m_color>("color"_hs).custom<FieldMeta>(FieldMeta{
                .m_displayName = "Color",
                .m_hint = FieldHint::Color})
            .data<&DirectionalLightComponent::m_intensity>("intensity"_hs).custom<FieldMeta>(FieldMeta{"Intensity"})
            .data<&DirectionalLightComponent::b_castShadow>("castShadow"_hs).custom<FieldMeta>(FieldMeta{"Cast Shadow"});

        RegisterComponent<SkyComponent>("Sky"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Sky",
            }
        });
        RegisterComponent<StaticMeshComponent>("StaticMesh"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Static Mesh",
            }
        });

        RegisterComponent<SkinnedMeshComponent>("SkinnedMesh"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Skinned Mesh",
            }
        });

        RegisterComponent<SkeletonComponent>("Skeleton"_hs);
        entt::meta_factory<SkeletonComponent>()
            .data<&SkeletonComponent::m_currentAnimation>("currentAnimation"_hs).custom<FieldMeta>(FieldMeta{"Current Animation"})
            .data<&SkeletonComponent::m_loop>("loop"_hs).custom<FieldMeta>(FieldMeta{"Loop"})
            .data<&SkeletonComponent::m_speed>("speed"_hs).custom<FieldMeta>(FieldMeta{"Speed"});

        RegisterComponent<SkeletonStateComponent>("SkeletonState"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Skeleton State",
            }
        });

        entt::meta_factory<SkeletonStateComponent>()
            .data<&SkeletonStateComponent::m_time>("time"_hs).custom<FieldMeta>(FieldMeta{"Time"});

        RegisterComponent<OrbitCameraControllerComponent>("OrbitCameraController"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Orbit Camera Controller",
                .b_writeable = true,
            }
        });

        entt::meta_factory<OrbitCameraControllerComponent>()
            .data<&OrbitCameraControllerComponent::m_orbitSpeed>("orbitSpeed"_hs).custom<FieldMeta>(FieldMeta{"Orbit Speed"})
            .data<&OrbitCameraControllerComponent::m_panSpeed>("panSpeed"_hs).custom<FieldMeta>(FieldMeta{"Pan Speed"})
            .data<&OrbitCameraControllerComponent::m_zoomSpeed>("zoomSpeed"_hs).custom<FieldMeta>(FieldMeta{"Zoom Speed"})
            .data<&OrbitCameraControllerComponent::m_minDistance>("minDistance"_hs).custom<FieldMeta>(FieldMeta{"Min Distance"})
            .data<&OrbitCameraControllerComponent::m_maxDistance>("maxDistance"_hs).custom<FieldMeta>(FieldMeta{"Max Distance"})
            .data<&OrbitCameraControllerComponent::m_target>("target"_hs).custom<FieldMeta>(FieldMeta{"Target"});

        RegisterComponent<SpriteComponent>("Sprite"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Sprite",
                .b_writeable = true,
            }
        });

        RegisterComponent<DummyTriangleComponent>("DummyTriangle"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Dummy Triangle",
            }
        });

        RegisterComponent<FirstPersonCameraControllerComponent>("FirstPersonCameraController"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "First Person Camera Controller",
                .b_writeable = true,
            }
        });

        RegisterComponent<NameComponent>("Name"_hs, MetaData{
            .m_componentMetaData = ComponentMetaData{
                .m_displayName = "Name",
                .b_showInEditor = false,
            }
        });

        entt::meta_factory<FirstPersonCameraControllerComponent>()
            .data<&FirstPersonCameraControllerComponent::m_moveSpeed>("moveSpeed"_hs).custom<FieldMeta>(FieldMeta{"Move Speed"})
            .data<&FirstPersonCameraControllerComponent::m_lookSensitivity>("lookSensitivity"_hs).custom<FieldMeta>(FieldMeta{"Look Sensitivity"});

        RegisterCtx<ShadowConfig>("ShadowConfig"_hs, MetaData{
            .m_ctxMetaData = CtxMetaData{
                .m_displayName = "Shadow Config",
                .b_writeable = true,
            }
        });

        entt::meta_factory<ShadowConfig>()
            .data<&ShadowConfig::m_shadowBiasBase>("shadowBiasBase"_hs).custom<FieldMeta>(FieldMeta{"Bias Base"})
            .data<&ShadowConfig::m_shadowBiasSlope>("shadowBiasSlope"_hs).custom<FieldMeta>(FieldMeta{"Bias Slope"})
            .data<&ShadowConfig::m_shadowBiasMax>("shadowBiasMax"_hs).custom<FieldMeta>(FieldMeta{"Bias Max"})
            .data<&ShadowConfig::m_shadowMapSize>("shadowMapSize"_hs).custom<FieldMeta>(FieldMeta{"Map Size"})
            .data<&ShadowConfig::m_shadowFarDistance>("shadowFarDistance"_hs).custom<FieldMeta>(FieldMeta{"Far Distance"})
            .data<&ShadowConfig::m_shadowCascadeLambda>("shadowCascadeLambda"_hs).custom<FieldMeta>(FieldMeta{"Cascade Lambda"})
            .data<&ShadowConfig::m_shadowBehind>("shadowBehind"_hs).custom<FieldMeta>(FieldMeta{"Shadow Behind"});

        RegisterCtx<RenderDebugConfig>("RenderDebugConfig"_hs, MetaData{
            .m_ctxMetaData = CtxMetaData{
                .m_displayName = "Render Debug Config",
                .b_writeable = true,
            }
        });

        entt::meta_factory<RenderDebugConfig>()
            .data<&RenderDebugConfig::m_mode>("mode"_hs).custom<FieldMeta>(FieldMeta{"Mode"});

        RegisterCtx<EditorPropertiesCtx>("EditorProperties"_hs, MetaData{
            .m_ctxMetaData = CtxMetaData{
                .b_showInEditor = false,
            }
        });
    }

    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {

        // Do sorting so that handlers for the same component/ctx are grouped together.
        graph.AddMessageNode([this, &params](
            JobContext&, 
            Pipe<AddComponentMetaSignal> addComponent,
            Pipe<UpdateComponentMetaSignal> updateComponent,
            Pipe<RemoveComponentMetaSignal> removeComponent,
            Pipe<AddCtxMetaSignal> addCtx,
            Pipe<UpdateCtxMetaSignal> updateCtx,
            Pipe<RemoveCtxMetaSignal> removeCtx) -> Error {

            addComponent.HandleSpan([](std::span<AddComponentMetaSignal> signals) {
                std::sort(signals.begin(), signals.end(), [](AddComponentMetaSignal const& a, AddComponentMetaSignal const& b) {
                    return a.m_component.type().id() < b.m_component.type().id();
                });
            });

            updateComponent.HandleSpan([](std::span<UpdateComponentMetaSignal> signals) {
                std::sort(signals.begin(), signals.end(), [](UpdateComponentMetaSignal const& a, UpdateComponentMetaSignal const& b) {
                    return a.m_component.type().id() < b.m_component.type().id();
                });
            });

            removeComponent.HandleSpan([](std::span<RemoveComponentMetaSignal> signals) {
                std::sort(signals.begin(), signals.end(), [](RemoveComponentMetaSignal const& a, RemoveComponentMetaSignal const& b) {
                    return a.m_type.id() < b.m_type.id();
                });
            });

            addCtx.HandleSpan([](std::span<AddCtxMetaSignal> signals) {
                std::sort(signals.begin(), signals.end(), [](AddCtxMetaSignal const& a, AddCtxMetaSignal const& b) {
                    return a.m_value.type().id() < b.m_value.type().id();
                });
            });

            updateCtx.HandleSpan([](std::span<UpdateCtxMetaSignal> signals) {
                std::sort(signals.begin(), signals.end(), [](UpdateCtxMetaSignal const& a, UpdateCtxMetaSignal const& b) {
                    return a.m_value.type().id() < b.m_value.type().id();
                });
            });

            removeCtx.HandleSpan([](std::span<RemoveCtxMetaSignal> signals) {
                std::sort(signals.begin(), signals.end(), [](RemoveCtxMetaSignal const& a, RemoveCtxMetaSignal const& b) {
                    return a.m_type.id() < b.m_type.id();
                });
            });

            return {};
        });

        return {};
    }

public:
    MetaDataModule() {
        Build();
    }

    std::string GetName() const override {
        return "MetaDataModule";
    }
};

std::unique_ptr<EngineModule> MetaDataModuleFactory::operator()() {
    return std::make_unique<MetaDataModule>();
}