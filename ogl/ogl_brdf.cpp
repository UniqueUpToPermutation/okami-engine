#include "ogl_brdf.hpp"
#include "ogl_texture.hpp"

#include "../paths.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLBrdfProvider::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IOGLCookTorrenceBRDFProvider>(this);

    return {};
}

Error OGLBrdfProvider::StartupImpl(InitContext const& context) {
    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLBrdfProvider");

    auto brdfLUT = CreateBrdfLut(*cache, m_debugMode);
    OKAMI_ERROR_RETURN(brdfLUT);

    m_brdfLut = std::move(*brdfLUT);

    return {};
}

Expected<GLTexture> okami::CreateBrdfLut(IGLShaderCache& cache, bool bDebugMode) {
    Error err;
    GLTexture brdfLut;

    auto blitShader = cache.LoadShader(GL_VERTEX_SHADER, GetGLSLShaderPath("blit.vs"));
    auto brdfShader = cache.LoadShader(GL_FRAGMENT_SHADER, GetGLSLShaderPath("cook_torrence_brdf.fs"));
    OKAMI_UNEXPECTED_RETURN(blitShader);
    OKAMI_UNEXPECTED_RETURN(brdfShader);
    
    auto program = CreateProgram(ProgramShaders{
        .m_vertex = **blitShader,
        .m_fragment = *brdfShader
    });
    OKAMI_UNEXPECTED_RETURN(program);

    LOG(INFO) << "Generating Cook-Torrence BRDF LUT...";

    glUseProgram(program->get());
    glGenTextures(1, brdfLut.ptr());
    glBindTexture(GL_TEXTURE_2D, brdfLut.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, kBrdfLutSize, kBrdfLutSize, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);

    auto uLutSize = glGetUniformLocation(program->get(), "u_LUTSize");
    glUniform2i(uLutSize, static_cast<GLint>(kBrdfLutSize), static_cast<GLint>(kBrdfLutSize));

    err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);

    glViewport(0, 0, kBrdfLutSize, kBrdfLutSize);

    // Create an empty VAO for rendering
    GLVertexArray vao;
    glGenVertexArrays(1, vao.ptr());
    glBindVertexArray(vao.get());

    err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);
    
    // Create a framebuffer for rendering the LUT
    GLFramebuffer fbo;
    glGenFramebuffers(1, fbo.ptr());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLut.get(), 0);

    err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);

    // Render a full-screen quad to generate the BRDF LUT
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);

    if (bDebugMode) {
        OKAMI_LOG_INFO("BRDF LUT generated with size " + std::to_string(kBrdfLutSize) + "x" + std::to_string(kBrdfLutSize));
        auto tex = FetchTextureFromGL(brdfLut.get());
        OKAMI_UNEXPECTED_RETURN(tex);
        OKAMI_LOG_INFO("BRDF LUT fetch successful for debugging purposes.");
        tex->SaveKTX2("brdf_lut_debug.ktx2");
    }

    return brdfLut;
}