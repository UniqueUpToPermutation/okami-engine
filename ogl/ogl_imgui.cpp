#include "ogl_imgui.hpp"

#include <imgui_impl_opengl3.h>

using namespace okami;

Error OGLImguiRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IImguiRenderer>(this);

    return {};
}

Error OGLImguiRenderer::StartupImpl(InitContext const& context) {
    m_imguiProvider = context.m_interfaces.Query<IImguiProvider>();

    if (m_imguiProvider) {
        ImGui_ImplOpenGL3_Init("#version 330");
    } else {
        OKAMI_LOG_WARNING("IImguiProvider interface not found; ImGui rendering will be disabled");
    }

    return {};
}
void OGLImguiRenderer::ShutdownImpl(InitContext const& context) {
    if (m_imguiProvider) {
        ImGui_ImplOpenGL3_Shutdown();
    }
}

Error OGLImguiRenderer::Pass(OGLPass const& pass) {
    if (!m_imguiProvider) {
        return {};
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplOpenGL3_RenderDrawData(&m_imguiProvider->GetDrawData());

    return {};
}

std::string OGLImguiRenderer::GetName() const {
    return "OpenGL ImGui Renderer";
}