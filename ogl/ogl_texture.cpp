#include "ogl_texture.hpp"
#include "../paths.hpp"

#include <glog/logging.h>
#include <glad/gl.h>

using namespace okami;

// ---------------------------------------------------------------------------
// GL format helpers (unchanged)
// ---------------------------------------------------------------------------

GLint okami::ToGlInternalFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:     return GL_R8;
        case TextureFormat::RG8:    return GL_RG8;
        case TextureFormat::RGB8:   return GL_RGB8;
        case TextureFormat::RGBA8:  return GL_RGBA8;
        case TextureFormat::R32F:   return GL_R32F;
        case TextureFormat::RG32F:  return GL_RG32F;
        case TextureFormat::RGB32F: return GL_RGB32F;
        case TextureFormat::RGBA32F:return GL_RGBA32F;
        default: return GL_INVALID_ENUM;
    }
}

GLenum okami::ToGlFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::R32F:   return GL_RED;
        case TextureFormat::RG8:
        case TextureFormat::RG32F:  return GL_RG;
        case TextureFormat::RGB8:
        case TextureFormat::RGB32F: return GL_RGB;
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA32F:return GL_RGBA;
        default: return GL_INVALID_ENUM;
    }
}

GLenum okami::ToGlType(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::RG8:
        case TextureFormat::RGB8:
        case TextureFormat::RGBA8:  return GL_UNSIGNED_BYTE;
        case TextureFormat::R32F:
        case TextureFormat::RG32F:
        case TextureFormat::RGB32F:
        case TextureFormat::RGBA32F:return GL_FLOAT;
        default: return GL_INVALID_ENUM;
    }
}

// ---------------------------------------------------------------------------
// OGLDeletionQueue
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// OGLTexture
// ---------------------------------------------------------------------------

OGLTexture::~OGLTexture() {
    if (m_deletion_queue) {
        if (GLuint id = m_texture.release(); id != 0) {
            m_deletion_queue->PushTexture(id);
        }
    }
    // If no queue (e.g. never assigned), ~GLTexture() runs normally
    // — only safe if destruction is already on the GL thread.
}

// ---------------------------------------------------------------------------
// Helper: upload a Texture to an OGLTexture on the GL thread
// ---------------------------------------------------------------------------

static Error UploadToGL(OGLTexture& out, Texture const& data) {
    auto const& desc = data.GetDesc();

    glGenTextures(1, out.m_texture.ptr());
    glBindTexture(GL_TEXTURE_2D, out.m_texture);

    for (int mip = 0; mip < static_cast<int>(desc.mipLevels); ++mip) {
        GLsizei w = static_cast<GLsizei>(std::max(1u, desc.width  >> mip));
        GLsizei h = static_cast<GLsizei>(std::max(1u, desc.height >> mip));

        glTexImage2D(GL_TEXTURE_2D, mip,
            ToGlInternalFormat(desc.format),
            w, h, 0,
            ToGlFormat(desc.format),
            ToGlType(desc.format),
            data.GetData(mip).data());
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        desc.mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    OKAMI_ERROR_RETURN(GET_GL_ERROR());

    out.m_desc = desc;
    out.m_loaded.store(true, std::memory_order_release);
    return {};
}

// ---------------------------------------------------------------------------
// OGLTextureManager
// ---------------------------------------------------------------------------

Error OGLTextureManager::RegisterImpl(InterfaceCollection& ic) {
    ic.Register<ITextureManager>(this);
    ic.RegisterSignalHandler<OnResourceLoadedEvent<Texture>>(&m_loaded_handler);
    return {};
}

TextureHandle OGLTextureManager::LoadTexture(
    std::filesystem::path const& path,
    TextureLoadParams             params,
    InterfaceCollection&          ic) {

    // --- Dedup: return existing handle if still alive ---
    {
        std::lock_guard lock(m_mtx);
        auto it = m_path_cache.find(path);
        if (it != m_path_cache.end()) {
            if (auto existing = it->second.lock()) {
                return existing; // already loading or loaded
            }
            m_path_cache.erase(it); // expired, fall through to create new
        }
    }

    // --- Pre-create OGLTexture (IsLoaded() = false until upload) ---
    auto id  = m_next_id.fetch_add(1, std::memory_order_relaxed);
    auto pending = std::make_unique<PendingLoad>();
    pending->m_texture            = std::make_shared<OGLTexture>();

    pending->m_texture->m_deletion_queue = m_deletion_queue;
    TextureHandle handle = pending->m_texture; // keep a copy before moving

    {
        std::lock_guard lock(m_mtx);
        m_path_cache[path] = pending->m_texture; // weak_ptr for dedup
        m_pending[id]      = std::move(pending);
    }

    ic.SendSignal(LoadResourceSignal<Texture>{
        .m_path   = path,
        .m_params = params,
        .m_id     = id,
    });

    return handle; // IsLoaded() = false; becomes true after ReceiveMessagesImpl
}

TextureHandle OGLTextureManager::CreateTexture(Texture data) {
    auto tex = std::make_shared<OGLTexture>();
    tex->m_deletion_queue = m_deletion_queue;
    auto err = UploadToGL(*tex, data);
    if (err.IsError()) {
        LOG(ERROR) << "OGLTextureManager::CreateTexture: GL upload failed: " << err;
    }
    return tex;
}

Error OGLTextureManager::ReceiveMessagesImpl(
    MessageBus& /*bus*/, RecieveMessagesParams const& /*params*/) {

    // Drain GL deletions deferred from non-GL threads.
    m_deletion_queue->Drain();

    Error err;

    m_loaded_handler.Handle([&](OnResourceLoadedEvent<Texture> msg) {
        if (!msg.m_data) {
            err += msg.m_data.error();
            LOG(ERROR) << "OGLTextureManager: texture load failed: " << msg.m_data.error();
            return;
        }

        auto id = msg.m_id;

        std::shared_ptr<OGLTexture> tex;
        {
            std::lock_guard lock(m_mtx);
            auto it = m_pending.find(id);
            if (it == m_pending.end()) {
                return; // handle was released before load completed
            }
            tex = it->second->m_texture;
            m_pending.erase(it); // no longer pending
        }

        err += UploadToGL(*tex, *msg.m_data);
    });

    return err;
}


// ---------------------------------------------------------------------------
// FetchTextureFromGL (unchanged)
// ---------------------------------------------------------------------------

Expected<Texture> okami::FetchTextureFromGL(GLuint textureId) {
    glBindTexture(GL_TEXTURE_2D, textureId);

    std::vector<std::pair<int,int>> mipSizes;
    for (int mip = 0; mip < 32; ++mip) {
        GLint w = 0, h = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_WIDTH,  &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_HEIGHT, &h);
        if (w <= 0 || h <= 0) break;
        mipSizes.emplace_back(w, h);
    }
    OKAMI_UNEXPECTED_RETURN_IF(mipSizes.empty(), "Failed to determine mip levels for texture");

    GLint internalFmt = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFmt);

    auto InternalToTextureFormat = [&](GLint fmt) -> Expected<TextureFormat> {
        switch (fmt) {
            case GL_R8:    return TextureFormat::R8;
            case GL_RG8:   return TextureFormat::RG8;
            case GL_RGB8:  return TextureFormat::RGB8;
            case GL_RGBA8: return TextureFormat::RGBA8;
            case GL_R32F:  return TextureFormat::R32F;
            case GL_RG32F: return TextureFormat::RG32F;
            case GL_RGB32F:return TextureFormat::RGB32F;
            case GL_RGBA32F:return TextureFormat::RGBA32F;
            case GL_R16F:  return TextureFormat::R32F;
            case GL_RG16F: return TextureFormat::RG32F;
            case GL_RGB16F:return TextureFormat::RGB32F;
            case GL_RGBA16F:return TextureFormat::RGBA32F;
            default:
                return OKAMI_UNEXPECTED("Unsupported internal format for texture fetch!");
        }
    };

    auto exTexFmt = InternalToTextureFormat(internalFmt);
    OKAMI_UNEXPECTED_RETURN(exTexFmt);
    TextureFormat texFmt = *exTexFmt;

    TextureDesc desc{};
    desc.type      = TextureType::TEXTURE_2D;
    desc.format    = texFmt;
    desc.width     = static_cast<uint32_t>(mipSizes[0].first);
    desc.height    = static_cast<uint32_t>(mipSizes[0].second);
    desc.depth     = 1;
    desc.arraySize = 1;
    desc.mipLevels = static_cast<uint32_t>(mipSizes.size());

    Texture out(desc);

    GLint prevPack = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &prevPack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    OKAMI_DEFER(glPixelStorei(GL_PACK_ALIGNMENT, prevPack));

    for (uint32_t mip = 0; mip < desc.mipLevels; ++mip) {
        GLint w = 0, h = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_WIDTH,  &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_HEIGHT, &h);
        OKAMI_UNEXPECTED_RETURN_IF(w <= 0 || h <= 0, "Unexpected mip query result");

        auto dest = out.GetData(mip, 0);
        OKAMI_UNEXPECTED_RETURN_IF(dest.size() == 0, "Destination buffer size is zero");

        glGetTexImage(GL_TEXTURE_2D, static_cast<GLint>(mip),
            ToGlFormat(desc.format), ToGlType(desc.format), dest.data());

        auto glErr = GET_GL_ERROR();
        OKAMI_UNEXPECTED_RETURN(glErr);
    }

    return out;
}
