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
#include "animation.hpp"

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

    std::string GetName() const override {
        return "MergeModule (Default for " + std::string(entt::resolve<ComponentT>().info().name()) + ")";
    }
};

template <typename T>
class DefaultCtxMergeModule final : public EngineModule {
protected:
    Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override {
        auto& registry = params.m_registry;

        bus.Handle<AddCtxSignal<T>>([&](AddCtxSignal<T> const& signal) {
            if (!registry.ctx().contains<T>()) {
                registry.ctx().emplace<T>(signal.m_value);
            }
        });

        bus.Handle<UpdateCtxSignal<T>>([&](UpdateCtxSignal<T> const& signal) {
            registry.ctx().erase<T>();
            registry.ctx().emplace<T>(signal.m_value);
        });

        bus.Handle<RemoveCtxSignal<T>>([&](RemoveCtxSignal<T> const&) {
            registry.ctx().erase<T>();
        });

        return {};
    }

    std::string GetName() const override {
        return "CtxMergeModule (Default for " + std::string(entt::resolve<T>().info().name()) + ")";
    }
};

class MetaDataModule final : public EngineModule {
protected:
    template <typename ComponentT>
    auto RegisterComponent(entt::hashed_string name, MetaData metaData = MetaData::ComponentDefault()) {
        if (metaData.IsComponent()) {
            if (metaData.m_componentMetaData->NeedsDefaultHandler()) {
                CreateChild<DefaultMergeModule<ComponentT>>();
            }
        }
        
        return entt::meta_factory<ComponentT>().custom<MetaData>(std::move(metaData)).type(name);
    }

        template <typename T>
        auto RegisterCtx(entt::hashed_string name, MetaData metaData = MetaData::CtxDefault()) {
            if (metaData.IsCtx()) {
                if (metaData.m_ctxMetaData->NeedsDefaultHandler()) {
                    CreateChild<DefaultCtxMergeModule<T>>();
                }
            }
            return entt::meta_factory<T>().custom<MetaData>(std::move(metaData)).type(name);
        }

    void Build() {
        entt::meta_reset();

        entt::meta_factory<glm::vec3>()
            .type("vec3"_hs);
        entt::meta_factory<glm::quat>()
            .type("quat"_hs);
        entt::meta_factory<glm::mat3>()
            .type("mat3"_hs);

        RegisterComponent<Camera>("Camera"_hs);
        RegisterComponent<Transform>("Transform"_hs);

        RegisterComponent<AmbientLightComponent>("AmbientLight"_hs);
        RegisterComponent<DirectionalLightComponent>("DirectionalLight"_hs);
        RegisterComponent<SkyComponent>("Sky"_hs);
        RegisterComponent<StaticMeshComponent>("StaticMesh"_hs);
        RegisterComponent<SkinnedMeshComponent>("SkinnedMesh"_hs);
        RegisterComponent<SkeletonComponent>("Skeleton"_hs);
        RegisterComponent<SkeletonStateComponent>("SkeletonState"_hs);

        RegisterComponent<OrbitCameraControllerComponent>("OrbitCameraController"_hs);
        RegisterComponent<SpriteComponent>("Sprite"_hs);
        RegisterComponent<DummyTriangleComponent>("DummyTriangle"_hs);
        RegisterComponent<FirstPersonCameraControllerComponent>("FirstPersonCameraController"_hs);

        RegisterCtx<ShadowConfig>("ShadowConfig"_hs);
        RegisterCtx<RenderDebugConfig>("RenderDebugConfig"_hs);
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