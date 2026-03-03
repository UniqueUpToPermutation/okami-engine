#pragma once

#include "../common.hpp"
#include "../texture.hpp"
#include "../content.hpp"

#include "ogl_utils.hpp"

namespace okami {
    GLint  ToGlInternalFormat(TextureFormat format);
    GLenum ToGlFormat(TextureFormat format);
    GLenum ToGlType(TextureFormat format);

    // Concrete OpenGL texture.  Heap-allocated and ref-counted via TextureHandle.
    // IsLoaded() is false until the async GPU upload completes.
    class OGLTexture final : public ITexture {
    public:
        GLTexture         m_texture;
        TextureDesc       m_desc{};
        std::atomic<bool> m_loaded{false};
        // Set by OGLTextureManager at construction; may be null for unit tests.
        std::shared_ptr<OGLDeletionQueue> m_deletion_queue;

        OGLTexture() = default;
        OKAMI_NO_COPY(OGLTexture);
        ~OGLTexture();

        TextureDesc const& GetDesc()  const override { return m_desc; }
        bool               IsLoaded() const override {
            return m_loaded.load(std::memory_order_acquire);
        }
    };

    // The single OGL texture manager.
    //
    // Textures are NOT stored in the entt registry.  Lifetime is managed
    // entirely through shared_ptr (TextureHandle).  Async file loading uses
    // the existing IO-thread signal machinery.
    class OGLTextureManager final :
        public EngineModule,
        public ITextureManager {
    private:
        // Tracks a single in-flight async load.
        struct PendingLoad {
            std::shared_ptr<OGLTexture> m_texture; // pre-created handle
        };

        std::shared_ptr<OGLDeletionQueue> m_deletion_queue =
            std::make_shared<OGLDeletionQueue>();

        std::mutex m_mtx;
        std::unordered_map<std::filesystem::path, std::weak_ptr<OGLTexture>, PathHash>
            m_path_cache;  // for path dedup
        std::unordered_map<uint32_t, std::unique_ptr<PendingLoad>>
            m_pending;     // in-flight loads
        std::atomic<uint32_t> m_next_id{1};

        DefaultSignalHandler<OnResourceLoadedEvent<Texture>> m_loaded_handler;

        Error RegisterImpl(InterfaceCollection& ic) override;
        Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override;

    public:
        // ITextureManager
        TextureHandle LoadTexture(
            std::filesystem::path const& path,
            TextureLoadParams            params,
            InterfaceCollection&         ic) override;
        TextureHandle CreateTexture(Texture data) override;

        // Convenience downcast – valid for any TextureHandle produced by this manager.
        static OGLTexture* GetOGLTexture(TextureHandle const& handle) {
            return static_cast<OGLTexture*>(handle.get());
        }

        std::string GetName() const override { return "OGL Texture Manager"; }
    };

    Expected<Texture> FetchTextureFromGL(GLuint textureId);
}
