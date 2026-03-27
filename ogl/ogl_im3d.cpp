#include "ogl_im3d.hpp"

#include "../log.hpp"

using namespace okami;

Error OGLIm3DRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    return {};
}
Error OGLIm3DRenderer::StartupImpl(InitContext const& context) {
    m_dataProvider = context.m_interfaces.Query<IIm3dProvider>();

    if (!m_dataProvider) {
        OKAMI_LOG_WARNING("IIm3dProvider interface not found; Im3d rendering will be disabled");
        return {};
    }

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider, "IOGLSceneGlobalsProvider interface not available for OGLIm3DRenderer");

    // Load shaders directly — no cache, these are the preprocessed im3d2 variants.
    auto pointsProgram = CreateProgram(ProgramShaderPaths{
        .m_vertex   = GetGLSLShaderPath("im3d.points.vs.glsl"),
        .m_fragment = GetGLSLShaderPath("im3d.points.fs.glsl"),
        .m_geometry = GetGLSLShaderPath("im3d.points.gs.glsl")
    });
    OKAMI_ERROR_RETURN(pointsProgram);
    m_programPoints = std::move(*pointsProgram);

    auto linesProgram = CreateProgram(ProgramShaderPaths{
        .m_vertex   = GetGLSLShaderPath("im3d.lines.vs.glsl"),
        .m_fragment = GetGLSLShaderPath("im3d.lines.fs.glsl"),
        .m_geometry = GetGLSLShaderPath("im3d.lines.gs.glsl")
    });
    OKAMI_ERROR_RETURN(linesProgram);
    m_programLines = std::move(*linesProgram);

    auto trianglesProgram = CreateProgram(ProgramShaderPaths{
        .m_vertex   = GetGLSLShaderPath("im3d.triangles.vs.glsl"),
        .m_fragment = GetGLSLShaderPath("im3d.triangles.fs.glsl")
    });
    OKAMI_ERROR_RETURN(trianglesProgram);
    m_programTriangles = std::move(*trianglesProgram);

    Error err;
    for (GLProgram* prog : {&m_programPoints, &m_programLines, &m_programTriangles}) {
        glUseProgram(prog->get());
        err += GET_GL_ERROR();
        err += AssignBufferBindingPoint(*prog, "SceneGlobalsBlock", BufferBindingPoints::SceneGlobals);
    }

    auto buffer = UploadVertexBuffer<glsl::Im3dVertex>::Create(
        glsl::__get_vs_input_infoIm3dVertex());
    OKAMI_ERROR_RETURN(buffer);
    m_vertexBuffer = std::move(*buffer);

    m_pipelineState = OGLPipelineState{
        .depthTestEnabled  = true,
        .depthMask         = true,   // don't write depth for transparent im3d geometry
        .blendEnabled      = true,
        .blendSrcRGB       = GL_SRC_ALPHA,
        .blendDstRGB       = GL_ONE_MINUS_SRC_ALPHA,
        .blendSrcAlpha     = GL_ONE,
        .blendDstAlpha     = GL_ONE_MINUS_SRC_ALPHA,
    };

    return err;
}

Error OGLIm3DRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    if (!m_dataProvider) {
        return {};
    }

    if (pass.m_type != OGLPassType::Forward) {
        return {};
    }

    auto const& im3dData = m_dataProvider->GetIm3dContext();
    if (!im3dData.m_context) {
        return {};
    }

    size_t vertexCount = 0;
    for (uint32_t i = 0; i < im3dData->getDrawListCount(); ++i) {
        auto const& drawCall = im3dData->getDrawLists()[i];
        vertexCount += drawCall.m_vertexCount;
    }

    Error err;
    // Upload data to GPU
    err += m_vertexBuffer.Reserve(vertexCount); 

    if (vertexCount == 0) {
        return err;
    }

    {
        auto map = m_vertexBuffer.Map();
        OKAMI_ERROR_RETURN_IF(!map, "Failed to map vertex buffer");

        size_t offset = 0;
        for (uint32_t i = 0; i < im3dData->getDrawListCount(); ++i) {
            auto const& drawCall = im3dData->getDrawLists()[i];
            std::memcpy(map->Data() + offset, drawCall.m_vertexData, drawCall.m_vertexCount * sizeof(glsl::Im3dVertex));
            offset += drawCall.m_vertexCount;
        }
    }

    err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);

    glBindVertexArray(m_vertexBuffer.GetVertexArray());
    err += GET_GL_ERROR();

    m_pipelineState.SetToGL();

    GLint currentVertex = 0;
    for (uint32_t i = 0; i < im3dData->getDrawListCount(); ++i) {
        auto const& drawCall = im3dData->getDrawLists()[i];

        GLProgram* prog = nullptr;
        GLint primitiveType = GL_POINTS;
        switch (drawCall.m_primType) {
            case Im3d::DrawPrimitive_Points:
                prog          = &m_programPoints;
                primitiveType = GL_POINTS;
                break;
            case Im3d::DrawPrimitive_Lines:
                prog          = &m_programLines;
                primitiveType = GL_LINES;
                break;
            case Im3d::DrawPrimitive_Triangles:
                prog          = &m_programTriangles;
                primitiveType = GL_TRIANGLES;
                break;
        }

        if (!prog) {
            currentVertex += drawCall.m_vertexCount;
            continue;
        }

        glUseProgram(prog->get());
        err += GET_GL_ERROR();

        glDrawArrays(primitiveType, currentVertex, drawCall.m_vertexCount);
        err += GET_GL_ERROR();
        currentVertex += drawCall.m_vertexCount;
    }

    return err;
}

std::string OGLIm3DRenderer::GetName() const { 
    return "OGL Im3d Module";
}