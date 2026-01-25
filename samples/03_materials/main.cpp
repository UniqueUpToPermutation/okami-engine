#include <iostream>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <filesystem>

#include "common.hpp"
#include "module.hpp"
#include "engine.hpp"

#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"

#include "renderer.hpp"
#include "transform.hpp"
#include "texture.hpp"
#include "paths.hpp"
#include "geometry.hpp"
#include "physics.hpp"
#include "storage.hpp"

using namespace okami;

int main() {
    Engine en;

    RendererParams params;

    // Register renderer based on compile-time options
    en.CreateModule<GLFWModuleFactory>();
    en.CreateModule<OGLRendererFactory>({}, params);

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    auto textureHandle = en.LoadResource<Texture>(GetAssetPath("test.ktx2"));
    auto geometryHandle = en.LoadResource<Geometry>(GetAssetPath("box.glb"));

    BasicTexturedMaterial material;
    material.m_colorTexture = textureHandle;
    auto materialHandle = en.CreateMaterial(material);

    auto boxEntity = en.CreateEntity();
    en.AddComponent(boxEntity, StaticMeshComponent{ 
        .m_geometry = geometryHandle, 
        .m_material = materialHandle 
    });

    auto cameraEntity = en.CreateEntity();
    en.AddComponent(cameraEntity, Camera::Orthographic(10.0f, -10.0f, 10.0f));
    en.AddComponent(cameraEntity, Transform::LookAt(
        glm::vec3(5.0f, 5.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));
    en.SetActiveCamera(cameraEntity);
    
    auto transformAccessor = en.QueryInterface<IComponentView<Transform>>();

    // Rotate the camera around the origin
    en.AddScript([&](
        JobContext& ctx,
        In<Time> inTime,
        Out<UpdateComponentSignal<Transform>> outTransform) -> Error {

        auto transform = transformAccessor->Get(cameraEntity);

        outTransform.Send(UpdateComponentSignal<Transform>{
            .m_entity = cameraEntity,
            .m_component = Transform::RotateY(0.5f * inTime->GetDeltaTimeF()) * transform
        });

        return {};
    });

    en.Run();
    en.Shutdown();
}
