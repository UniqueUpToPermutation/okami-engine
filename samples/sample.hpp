#pragma once

#include "engine.hpp"
#include "glfw_module.hpp"

#include <iostream>
#include <optional>

namespace okami {

// ---------------------------------------------------------------------------
// Base class for all samples.
//
// Each sample implements SetupModules() to register engine modules and
// SetupScene() to add entities, components, and scripts.  Both methods are
// called by RunSample<T>() (standalone mode) and by the headless renderer
// tests (headless mode).
// ---------------------------------------------------------------------------
class Sample {
public:
    virtual ~Sample() = default;

    virtual void SetupModules(Engine& en, std::optional<HeadlessGLParams> headless = {}) = 0;
    virtual void SetupScene(Engine& en) = 0;
    virtual size_t GetTestFrameCount() const { return 1; }
};

// Adds ImGui + Editor modules and a tilde-key toggle script so that any
// sample built with RunSample can bring up the editor by pressing `~`.
// Implemented in sample.cpp (compiled into every sample executable).
void InstallEditorModules(Engine& en);

// ---------------------------------------------------------------------------
// Driver function used by each sample's main.cpp.
//
//   int main() { return okami::RunSample<MySample>(); }
// ---------------------------------------------------------------------------
template <typename TSample>
int RunSample() {
    TSample sample;
    Engine en;

    InstallEditorModules(en);
    sample.SetupModules(en);

    Error err = en.Startup();
    if (err.IsError()) {
        std::cerr << "Engine startup failed: " << err << std::endl;
        return 1;
    }

    sample.SetupScene(en);
    en.Run();
    en.Shutdown();
    return 0;
}

} // namespace okami
