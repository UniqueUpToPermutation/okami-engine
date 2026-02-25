#include "meta.hpp"

#include <entt/meta/factory.hpp>
#include <entt/meta/resolve.hpp>

#include "camera.hpp"
#include "transform.hpp"
#include "light.hpp"
#include "sky.hpp"
#include "renderer.hpp"
#include "camera_controllers.hpp"

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

        RegisterComponent<OrbitCameraControllerComponent>("OrbitCameraController"_hs);
        RegisterComponent<SpriteComponent>("Sprite"_hs);
        RegisterComponent<DummyTriangleComponent>("DummyTriangle"_hs);
    }

public:
    MetaDataModule() {
        Build();
    }
};

std::unique_ptr<EngineModule> MetaDataModuleFactory::operator()() {
    return std::make_unique<MetaDataModule>();
}