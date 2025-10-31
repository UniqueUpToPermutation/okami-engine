#include "ogl_static_mesh.hpp"

using namespace okami;

Error OGLStaticMeshRenderer::RegisterImpl(ModuleInterface& mi) {
    return {};
}

Error OGLStaticMeshRenderer::StartupImpl(ModuleInterface& mi) {
    return {};
}

void OGLStaticMeshRenderer::ShutdownImpl(ModuleInterface& mi) {

}

Error OGLStaticMeshRenderer::ProcessFrameImpl(Time const&, ModuleInterface&) {
    return {};
}

Error OGLStaticMeshRenderer::MergeImpl() {
    return {};
}

OGLStaticMeshRenderer::OGLStaticMeshRenderer(OGLGeometryManager* geometryManager) :
    m_geometryManager(geometryManager) {
}

Error OGLStaticMeshRenderer::Pass(OGLPass const& pass) {
    return {};
}

std::string_view OGLStaticMeshRenderer::GetName() const {
    return "OGL Static Mesh Renderer";
}