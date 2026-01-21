#include "ogl_triangle.hpp"

using namespace okami;

Error OGLTriangleRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    return {};
}

Error OGLTriangleRenderer::StartupImpl(InitContext const& context) {
    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLTriangleRenderer");

    m_transformView = context.m_interfaces.Query<IComponentView<Transform>>();
    OKAMI_ERROR_RETURN_IF(!m_transformView, "IComponentView<Transform> interface not available for OGLTriangleRenderer");

    auto program = CreateProgram(ProgramShaderPaths{
        .m_vertex = GetGLSLShaderPath("triangle.vs"),
        .m_fragment = GetGLSLShaderPath("triangle.fs")
    }, *cache);
    OKAMI_ERROR_RETURN(program);

    m_program = std::move(*program);

    auto sceneUBO = UniformBuffer<glsl::SceneGlobals>::Create();
    OKAMI_ERROR_RETURN(sceneUBO);
    m_sceneUBO = std::move(*sceneUBO);

    Error err;
    glUseProgram(m_program.get());
    err += GET_GL_ERROR();
    err += AssignBufferBindingPoint(m_program, "SceneGlobalsBlock", BufferBindingPoints::SceneGlobals);

    // Create and setup VAO (required for modern OpenGL)
    GLuint vaoId;
    glGenVertexArrays(1, &vaoId);
    m_vao = GLVertexArray(vaoId);
    
    // Bind VAO - we don't need vertex buffers since we generate vertices in the shader
    glBindVertexArray(m_vao.get());
    glBindVertexArray(0); // Unbind

    u_world = GetUniformLocation(m_program, "u_world", err);

    m_pipelineState = OGLPipelineState{
        .depthTestEnabled = true,
    };

    return err;
}

void OGLTriangleRenderer::ShutdownImpl(InitContext const& context) {

}

Error OGLTriangleRenderer::Pass(OGLPass const& pass) {
    if (pass.m_type != OGLPassType::Forward) {
        return {};
    }

    Error err;

    m_pipelineState.SetToGL();
    glUseProgram(m_program.get()); OKAMI_CHK_GL;
    glBindVertexArray(m_vao.get()); OKAMI_CHK_GL;
    
    // Set view-projection matrix uniform
    err += m_sceneUBO.Bind(BufferBindingPoints::SceneGlobals);
    err += m_sceneUBO.Write(pass.m_sceneGlobals);

    m_storage->ForEach(
        [&](entity_t entity, DummyTriangleComponent const&) {
            auto world = m_transformView->GetOr(entity, Transform::Identity()).AsMatrix();
            glUniformMatrix4fv(u_world, 1, GL_FALSE, &world[0][0]);
            
            // Draw triangle (3 vertices)
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    );
    
    return err;
}

std::string OGLTriangleRenderer::GetName() const {
    return "OpenGL Triangle Renderer";
}

OGLTriangleRenderer::OGLTriangleRenderer() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}