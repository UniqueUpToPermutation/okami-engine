#include "bgfx_triangle.hpp"

#include "../paths.hpp"

using namespace okami;

Error BGFXTriangleModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error BGFXTriangleModule::StartupImpl(ModuleInterface&) {
    //GetBGFXShaderPath(""

    return {};
}

void BGFXTriangleModule::ShutdownImpl(ModuleInterface&) {

}

Error BGFXTriangleModule::ProcessFrameImpl(Time const&, ModuleInterface&) {
    return {};
}

Error BGFXTriangleModule::MergeImpl() {
    return {};
}
    
std::string_view BGFXTriangleModule::GetName() const {
    return "BGFX Triangle Module";
}