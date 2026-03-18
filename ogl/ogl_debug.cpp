#include "ogl_debug.hpp"

#include "../renderer.hpp"
#include "../entity_manager.hpp"
#include "../imgui.hpp"
#include "../jobs.hpp"

#include <glog/logging.h>

#include <imgui.h>

using namespace okami;

Error OGLDebugModule::RegisterImpl(InterfaceCollection& interfaces) {
    RegisterConfig<RenderDebugConfig>(interfaces, LOG_WRAP(WARNING));
    return {};
}

Error OGLDebugModule::StartupImpl(InitContext const& context) {
    auto cfg = ReadConfig<RenderDebugConfig>(context.m_interfaces, LOG_WRAP(WARNING));
    
    context.m_registry.ctx().emplace<RenderDebugConfig>(cfg);
    return {};
}
