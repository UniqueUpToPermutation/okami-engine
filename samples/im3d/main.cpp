#include "scene.hpp"

using namespace okami;

int main() {
    Engine en;
    sample_im3d::SetupModules(en);

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    sample_im3d::SetupScene(en);

    en.Run();
    en.Shutdown();
}
