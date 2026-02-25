#include "engine.hpp"

#include "ogl/ogl_renderer.hpp"
#include "glfw_module.hpp"

#include "transform.hpp"
#include "paths.hpp"
#include "geometry.hpp"

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

    auto geometryHandle = en.LoadResource<Geometry>(GetAssetPath("box.glb"));

    auto cameraEntity = en.CreateEntity();
    en.AddComponent(cameraEntity, Camera::Orthographic(3.0f, -3.0f, 3.0f));
    en.AddComponent(cameraEntity, Transform::LookAt(
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));
    en.SetActiveCamera(cameraEntity);

    auto meshEntity = en.CreateEntity();
    en.AddComponent(meshEntity, StaticMeshComponent{ geometryHandle });
    en.AddComponent(meshEntity, Transform::Scale(0.25f));

    // Rotate the camera around the origin
    en.AddScript([&](
        JobContext& ctx,
        In<Time> inTime,
        Out<UpdateComponentSignal<Transform>> outTransform) -> Error {

        auto transform = en.GetRegistry().get<Transform>(cameraEntity);

        outTransform.Send(UpdateComponentSignal<Transform>{
            .m_entity = cameraEntity,
            .m_component = Transform::RotateY(0.5f * inTime->GetDeltaTimeF()) * transform
        });

        return {};
    });

    en.Run();
    en.Shutdown();
}
