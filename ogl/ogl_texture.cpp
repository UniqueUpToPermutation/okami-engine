#include "ogl_texture.hpp"

#include <glad/gl.h>

using namespace okami;

GLint okami::ToGlInternalFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8: return GL_R8;
        case TextureFormat::RG8: return GL_RG8;
        case TextureFormat::RGB8: return GL_RGB8;
        case TextureFormat::RGBA8: return GL_RGBA8;
        case TextureFormat::R32F: return GL_R32F;
        case TextureFormat::RG32F: return GL_RG32F;
        case TextureFormat::RGB32F: return GL_RGB32F;
        case TextureFormat::RGBA32F: return GL_RGBA32F;
        default: return GL_INVALID_ENUM;
    }
}

GLenum okami::ToGlFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8: return GL_RED;
        case TextureFormat::RG8: return GL_RG;
        case TextureFormat::RGB8: return GL_RGB;
        case TextureFormat::RGBA8: return GL_RGBA;
        case TextureFormat::R32F: return GL_RED;
        case TextureFormat::RG32F: return GL_RG;
        case TextureFormat::RGB32F: return GL_RGB;
        case TextureFormat::RGBA32F: return GL_RGBA;
        default: return GL_INVALID_ENUM;
    }
}

GLenum okami::ToGlType(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::RG8:
        case TextureFormat::RGB8:
        case TextureFormat::RGBA8:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::R32F:
        case TextureFormat::RG32F:
        case TextureFormat::RGB32F:
        case TextureFormat::RGBA32F:
            return GL_FLOAT;
        default:
            return GL_INVALID_ENUM;
    }
}

Expected<std::pair<typename Texture::Desc, TextureImpl>> 
    OGLTextureManager::CreateResource(Texture&& data, std::any userData) {

    TextureImpl impl;
    glGenTextures(1, impl.m_texture.ptr());
    glBindTexture(GL_TEXTURE_2D, impl.m_texture);

    auto desc = data.GetDesc();

    for (int mip = 0; mip < static_cast<int>(desc.mipLevels); ++mip) {
        // Calculate mip level dimensions
        GLsizei mipWidth = static_cast<GLsizei>(std::max(1u, desc.width >> mip));
        GLsizei mipHeight = static_cast<GLsizei>(std::max(1u, desc.height >> mip));
        
        glTexImage2D(GL_TEXTURE_2D, mip, 
            ToGlInternalFormat(desc.format), 
            mipWidth, 
            mipHeight, 
            0, 
            ToGlFormat(desc.format), 
            ToGlType(desc.format), 
            data.GetData(mip).data());
    }

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    OKAMI_UNEXPECTED_RETURN(GetGlError());

    return std::make_pair(data.GetDesc(), std::move(impl));
}