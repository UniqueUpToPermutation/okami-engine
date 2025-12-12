#include "ogl_im3d.hpp"

#include "../log.hpp"

using namespace okami;

Error OGLIm3D::RegisterImpl(InterfaceCollection& interfaces) {
    return {};
}
Error OGLIm3D::StartupImpl(InitContext const& context) {
    m_dataProvider = context.m_interfaces.Query<IIm3dProvider>();

    if (!m_dataProvider) {
        OKAMI_LOG_WARNING("IIm3dProvider interface not available for OGLIm3D");
        return {};
    }

    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLIm3D");

    auto program = CreateProgram(ProgramShaderPaths{
        .m_vertex = GetGLSLShaderPath("im3d.vs"),
        .m_fragment = GetGLSLShaderPath("triangle.fs")
    }, *cache);
    OKAMI_ERROR_RETURN(program);

    m_program = std::move(*program);

    auto buffer = UploadVertexBuffer<glsl::Im3dVertex>::Create(
        glsl::__get_vs_input_infoIm3dVertex());
    OKAMI_ERROR_RETURN(buffer);
    m_vertexBuffer = std::move(*buffer);

    auto sceneUBO = UniformBuffer<glsl::SceneGlobals>::Create(
        m_program, "SceneGlobalsBlock", 0);
    OKAMI_ERROR_RETURN(sceneUBO);
    m_sceneUBO = std::move(*sceneUBO);

    m_pipelineState = OGLPipelineState{
        .depthTestEnabled = true,
        .program = m_program,
    };

    return {};
}
void OGLIm3D::ShutdownImpl(InitContext const& context) { }

Error OGLIm3D::ProcessFrameImpl(Time const& time, ExecutionContext const& context) { return {}; }
Error OGLIm3D::MergeImpl(MergeContext const& context) { return {}; }
Error OGLIm3D::Pass(OGLPass const& pass) {
    if (!m_dataProvider) {
        return {};
    }

    if (pass.m_type != OGLPassType::Forward) {
        return {};
    }

    m_pipelineState.SetToGL(); OKAMI_CHK_GL;

    auto const& im3dData = m_dataProvider->GetIm3dContext();

    size_t vertexCount = 0;
    for (uint32_t i = 0; i < im3dData.getDrawListCount(); ++i) {
        auto const& drawCall = im3dData.getDrawLists()[i];
        vertexCount += drawCall.m_vertexCount;
    }

    // Upload data to GPU
    m_vertexBuffer.Reserve(vertexCount); OKAMI_CHK_GL;
    {
        auto map = m_vertexBuffer.Map();
        OKAMI_ERROR_RETURN_IF(!map, "Failed to map vertex buffer");

        vertexCount = 0;
        for (uint32_t i = 0; i < im3dData.getDrawListCount(); ++i) {
            auto const& drawCall = im3dData.getDrawLists()[i];
            std::memcpy(map->Data() + vertexCount, drawCall.m_vertexData, drawCall.m_vertexCount * sizeof(glsl::Im3dVertex));
            vertexCount += drawCall.m_vertexCount;
        }
    }

    // Set uniforms
    m_sceneUBO.Bind();
    OKAMI_DEFER(m_sceneUBO.Unbind()); OKAMI_CHK_GL;
    m_sceneUBO.Write(pass.m_sceneGlobals); OKAMI_CHK_GL;

    glBindVertexArray(m_vertexBuffer.GetVertexArray()); OKAMI_CHK_GL;
    OKAMI_DEFER(glBindVertexArray(0));

    GLint currentVertex = 0;
    for (uint32_t i = 0; i < im3dData.getDrawListCount(); ++i) {
        auto const& drawCall = im3dData.getDrawLists()[i];

        GLint primitiveType = GL_POINTS;
        switch (drawCall.m_primType) {
            case Im3d::DrawPrimitive_Points:
                primitiveType = GL_POINTS;
                break;
            case Im3d::DrawPrimitive_Lines:
                primitiveType = GL_LINES;
                break;
            case Im3d::DrawPrimitive_Triangles:
                primitiveType = GL_TRIANGLES;
                break;
        }

        glDrawArrays(primitiveType, currentVertex, drawCall.m_vertexCount); OKAMI_CHK_GL;
        currentVertex += drawCall.m_vertexCount;
    }

    return {};
}

std::string OGLIm3D::GetName() const { 
    return "OGL Im3d Module";
}