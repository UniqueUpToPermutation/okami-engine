#include "ogl_material.hpp"

using namespace okami;

Error OGLBasicTexturedMaterialBindings::FetchBindings(GLProgram const& program) {
    glGetUniformLocation(program.get(), "u_diffuseMap"); OKAMI_CHK_GL;
    glGetUniformLocation(program.get(), "u_color"); OKAMI_CHK_GL;
    return {};
}

bool OGLBasicTexturedMaterialBindings::Bind(MaterialHandle handle, OGLTextureManager const& textureManager) const {
    auto const* material = m_manager->GetMaterialInfo(handle);
    auto const* textureImpl = textureManager.GetImpl(material->m_material.m_colorTexture);
    if (!material || !textureImpl) {
        return false;
    }

    glUniform4fv(m_color, 1, &material->m_material.m_colorTint[0]);
    glUniform1i(m_diffuseMap, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureImpl->m_texture.get());
    return true;
}

ProgramShaderPaths OGLBasicTexturedMaterialManager::GetShaderPaths() const {
    return ProgramShaderPaths{
        .m_fragment = GetGLSLShaderPath("basic_textured.frag")
    };
}
Error OGLBasicTexturedMaterialManager::RegisterImpl(InterfaceCollection& interfaces) {
    MaterialModuleBase<BasicTexturedMaterial, std::monostate>::RegisterImpl(interfaces);
    interfaces.Register<IOGLMaterialManager>(this);
    return {};
}
std::unique_ptr<OGLMaterialBindings> OGLBasicTexturedMaterialManager::CreateBinding() const {
    return std::make_unique<OGLBasicTexturedMaterialBindings>(this);
}
std::type_index OGLBasicTexturedMaterialManager::GetMaterialType() const {
    return typeid(BasicTexturedMaterial);
}