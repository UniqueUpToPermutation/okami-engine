#include "ogl_material.hpp"

using namespace okami;


Error OGLDefaultMaterialManager::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IOGLMaterialManager>(this);
    return {};
}

Error OGLDefaultMaterialManager::StartupImpl(InitContext const& context) {
    return {};
}

ProgramShaderPaths OGLDefaultMaterialManager::GetShaderPaths() const {
    return ProgramShaderPaths{
        .m_fragment = GetGLSLShaderPath("material_default.fs")
    };
}

Error OGLDefaultMaterialManager::Bind(MaterialHandle handle, OGLMaterialBindParams const& params) const {
    return {};
}

Error OGLDefaultMaterialManager::OnProgramCreated(GLProgram const& program, 
    OGLMaterialBindParams const& params) const {
    return {};
}



Error OGLBasicTexturedMaterialManager::RegisterImpl(InterfaceCollection& interfaces) {
    MaterialModuleBase<BasicTexturedMaterial, EmptyMaterialImpl>::RegisterImpl(interfaces);
    interfaces.Register<IOGLMaterialManager>(this);
    return {};
}

EmptyMaterialImpl OGLBasicTexturedMaterialManager::CreateImpl(BasicTexturedMaterial const& material) {
    return EmptyMaterialImpl{};
}

void OGLBasicTexturedMaterialManager::DestroyImpl(EmptyMaterialImpl& impl) {
    // No resources to clean up
}

ProgramShaderPaths OGLBasicTexturedMaterialManager::GetShaderPaths() const {
    return ProgramShaderPaths{
        .m_fragment = GetGLSLShaderPath("material_basic_textured.fs")
    };
}

Error OGLBasicTexturedMaterialManager::Bind(MaterialHandle handle, OGLMaterialBindParams const& params) const {
    auto materialInfo = GetMaterialInfo(handle.GetEntity());
    OKAMI_ERROR_RETURN_IF(!materialInfo, "OGLBasicTexturedMaterialManager: Material info not found for entity");
    
    auto textureImpl = m_textureManager->GetImpl(materialInfo->m_colorTexture);
    OKAMI_ERROR_RETURN_IF(!textureImpl, "OGLBasicTexturedMaterialManager: Texture implementation not found");
    
    glActiveTexture(GL_TEXTURE0 + params.m_startTextureBindPoint + static_cast<GLint>(TextureUnits::ColorTexture));
    glBindTexture(GL_TEXTURE_2D, textureImpl->m_texture.get());
    return GET_GL_ERROR();
}

Error OGLBasicTexturedMaterialManager::OnProgramCreated(
    GLProgram const& program,   
    OGLMaterialBindParams const& params) const {
    glUseProgram(program.get()); OKAMI_CHK_GL;
    return AssignTextureBindingPoint(program, "u_diffuseMap",  
        params.m_startTextureBindPoint, TextureUnits::ColorTexture);
}