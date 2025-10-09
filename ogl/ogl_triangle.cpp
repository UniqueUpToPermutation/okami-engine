#include "ogl_triangle.hpp"

using namespace okami;

Error OGLTriangleRenderer::RegisterImpl(ModuleInterface& mi) {
    return {};
}

Error OGLTriangleRenderer::StartupImpl(ModuleInterface& mi) {
    auto* cache = mi.m_interfaces.Query<IGLShaderCache>();
    if (!cache) {
        return Error("IGLShaderCache interface not available for OGLTriangleRenderer");
    }

    m_transformView = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!m_transformView) {
        return Error("IComponentView<Transform> interface not available for OGLTriangleRenderer");
    }

    auto program = CreateProgram(ProgramShaderPaths{
        .m_vertex = GetGLSLShaderPath("triangle.vs"),
        .m_fragment = GetGLSLShaderPath("triangle.fs")
    }, *cache);
    OKAMI_ERROR_RETURN(program);

    m_program = std::move(*program);

    // Create and setup VAO (required for modern OpenGL)
    GLuint vaoId;
    glGenVertexArrays(1, &vaoId);
    m_vao = GLVertexArray(vaoId);
    
    // Bind VAO - we don't need vertex buffers since we generate vertices in the shader
    glBindVertexArray(m_vao.get());
    glBindVertexArray(0); // Unbind

    Error uniformErrs;
    u_world = GetUniformLocation(m_program, "u_world", uniformErrs);
    u_viewProj = GetUniformLocation(m_program, "u_viewProj", uniformErrs);
    OKAMI_ERROR_RETURN(uniformErrs);

    return {};
}

void OGLTriangleRenderer::ShutdownImpl(ModuleInterface& mi) {

}

Error OGLTriangleRenderer::ProcessFrameImpl(Time const&, ModuleInterface& mi) {
    return {};
}

Error OGLTriangleRenderer::MergeImpl() {
    return {};
}

Error OGLTriangleRenderer::Pass(OGLPass const& pass) {
    glUseProgram(m_program.get());
    
    // Bind VAO
    glBindVertexArray(m_vao.get());
    
    // Set view-projection matrix uniform
    glUniformMatrix4fv(u_viewProj, 1, GL_FALSE, &pass.m_viewProjection[0][0]);

    m_storage->ForEach(
        [&](entity_t entity, DummyTriangleComponent const&) {
            auto world = m_transformView->GetOr(entity, Transform::Identity()).AsMatrix();
            glUniformMatrix4fv(u_world, 1, GL_FALSE, &world[0][0]);
            
            // Draw triangle (3 vertices)
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    );
    
    // Unbind VAO
    glBindVertexArray(0);

    return {};
}

std::string_view OGLTriangleRenderer::GetName() const {
    return "OpenGL Triangle Renderer";
}

OGLTriangleRenderer::OGLTriangleRenderer() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}