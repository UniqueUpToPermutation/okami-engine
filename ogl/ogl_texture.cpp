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
    OGLTextureManager::CreateResource(Texture&& data) {

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

    auto err = GET_GL_ERROR();
    OKAMI_UNEXPECTED_RETURN(err);

    return std::make_pair(data.GetDesc(), std::move(impl));
}

void OGLTextureManager::DestroyResourceImpl(TextureImpl& impl) {
    impl.m_texture = {};
}

Expected<Texture> okami::FetchTextureFromGL(GLuint textureId) {
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Determine mip levels by querying width until zero
    std::vector<std::pair<int,int>> mipSizes;
    for (int mip = 0; mip < 32; ++mip) {
        GLint w = 0, h = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_HEIGHT, &h);
        if (w <= 0 || h <= 0) break;
        mipSizes.emplace_back(w, h);
    }
    OKAMI_UNEXPECTED_RETURN_IF(mipSizes.empty(), "Failed to determine mip levels for texture");

    // Query internal format from level 0
    GLint internalFmt = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFmt);

    auto InternalToTextureFormat = [&](GLint fmt) -> Expected<TextureFormat> {
        switch (fmt) {
            case GL_R8: return TextureFormat::R8;
            case GL_RG8: return TextureFormat::RG8;
            case GL_RGB8: return TextureFormat::RGB8;
            case GL_RGBA8: return TextureFormat::RGBA8;
            case GL_R32F: return TextureFormat::R32F;
            case GL_RG32F: return TextureFormat::RG32F;
            case GL_RGB32F: return TextureFormat::RGB32F;
            case GL_RGBA32F: return TextureFormat::RGBA32F;
            // Support 16-bit float internal formats by mapping them to 32-bit float
            // storage in the engine. glGetTexImage can convert half->float when
            // we request GL_FLOAT as the read type.
            case GL_R16F: return TextureFormat::R32F;
            case GL_RG16F: return TextureFormat::RG32F;
            case GL_RGB16F: return TextureFormat::RGB32F;
            case GL_RGBA16F: return TextureFormat::RGBA32F;
            default: 
                return OKAMI_UNEXPECTED("Unsupported internal format for texture fetch!");
        }
    };

    Expected<TextureFormat> exTexFmt = InternalToTextureFormat(internalFmt);
    OKAMI_UNEXPECTED_RETURN(exTexFmt);
    TextureFormat texFmt = *exTexFmt;

    TextureDesc desc = {};
    desc.type = TextureType::TEXTURE_2D;
    desc.format = texFmt;
    desc.width = static_cast<uint32_t>(mipSizes[0].first);
    desc.height = static_cast<uint32_t>(mipSizes[0].second);
    desc.depth = 1;
    desc.arraySize = 1;
    desc.mipLevels = static_cast<uint32_t>(mipSizes.size());

    Texture out(desc);

    // Ensure tight packing
    GLint prevPack = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &prevPack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    OKAMI_DEFER(glPixelStorei(GL_PACK_ALIGNMENT, prevPack));

    for (uint32_t mip = 0; mip < desc.mipLevels; ++mip) {
        GLint w = 0, h = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_HEIGHT, &h);
        OKAMI_UNEXPECTED_RETURN_IF(w <= 0 || h <= 0, "Unexpected mip query result");

        auto dest = out.GetData(mip, 0);
        OKAMI_UNEXPECTED_RETURN_IF(dest.size() == 0, "Destination buffer size is zero");

        GLenum glFmt = ToGlFormat(desc.format);
        GLenum glType = ToGlType(desc.format);

        glGetTexImage(GL_TEXTURE_2D, static_cast<GLint>(mip), glFmt, glType, dest.data());

        // Optionally check GL errors here (omitted for brevity)
        auto err = GET_GL_ERROR();
        OKAMI_UNEXPECTED_RETURN(err);
    }

    return out;
}