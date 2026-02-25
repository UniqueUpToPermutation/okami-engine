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

    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLIm3DRenderer");

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

    Error err;
    glUseProgram(m_program.get());
    err += GET_GL_ERROR();
    err += AssignBufferBindingPoint(m_program, "SceneGlobalsBlock", BufferBindingPoints::SceneGlobals);

    m_pipelineState = OGLPipelineState{
        .depthTestEnabled = true,
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

    m_pipelineState.SetToGL();
    glUseProgram(m_program.get());
    err += GET_GL_ERROR();
    
    {
        auto map = m_vertexBuffer.Map();
        OKAMI_ERROR_RETURN_IF(!map, "Failed to map vertex buffer");

        vertexCount = 0;
        for (uint32_t i = 0; i < im3dData->getDrawListCount(); ++i) {
            auto const& drawCall = im3dData->getDrawLists()[i];
            std::memcpy(map->Data() + vertexCount, drawCall.m_vertexData, drawCall.m_vertexCount * sizeof(glsl::Im3dVertex));
            vertexCount += drawCall.m_vertexCount;
        }
    }

    // Set uniforms
    err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);

    glBindVertexArray(m_vertexBuffer.GetVertexArray());
    err += GET_GL_ERROR();

    GLint currentVertex = 0;
    for (uint32_t i = 0; i < im3dData->getDrawListCount(); ++i) {
        auto const& drawCall = im3dData->getDrawLists()[i];

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

        glDrawArrays(primitiveType, currentVertex, drawCall.m_vertexCount);
        err += GET_GL_ERROR();
        currentVertex += drawCall.m_vertexCount;
    }

    return err;
}

std::string OGLIm3DRenderer::GetName() const { 
    return "OGL Im3d Module";
}