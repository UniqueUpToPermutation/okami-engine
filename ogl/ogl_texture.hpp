#pragma once

#include "../common.hpp"
#include "../texture.hpp"
#include "../content.hpp"

#include "ogl_utils.hpp"

namespace okami {
    GLint ToGlInternalFormat(TextureFormat format);
    GLenum ToGlFormat(TextureFormat format);
    GLenum ToGlType(TextureFormat format);
    
    struct TextureImpl {
        // OpenGL texture ID
        GLTexture m_texture;
    };

    class OGLTextureManager : public ContentModule<Texture, TextureImpl> {
    public:
        Expected<std::pair<typename Texture::Desc, TextureImpl>> 
            CreateResource(Texture&& data) override;
        void DestroyResourceImpl(TextureImpl& impl) override;   
    };
}