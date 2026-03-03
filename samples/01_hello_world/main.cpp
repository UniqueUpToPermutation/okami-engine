#include "scene.hpp"

using namespace okami;

int main() {
    Engine en;
    sample_hello_world::SetupModules(en);

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    auto ctx = sample_hello_world::SetupScene(en);

    // Rotate the camera around the origin
    en.AddScript([&](
        JobContext& job,
        In<Time> inTime,
        Out<UpdateComponentSignal<Transform>> outTransform) -> Error {

        auto transform = en.GetRegistry().get<Transform>(ctx.cameraEntity);

        outTransform.Send(UpdateComponentSignal<Transform>{
            .m_entity    = ctx.cameraEntity,
            .m_component = Transform::RotateY(0.5f * inTime->GetDeltaTimeF()) * transform
        });

        return {};
    });

    en.Run();
    en.Shutdown();
}
