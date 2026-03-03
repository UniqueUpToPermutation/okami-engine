#pragma once

#include "ogl_utils.hpp"

#include "../geometry.hpp"
#include "../content.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace okami {
    struct PrimitiveImpl {
        GLVertexArray m_vao;
    };

    // Concrete OpenGL geometry.  Heap-allocated and ref-counted via GeometryHandle.
    // IsLoaded() is false until the async GL upload completes.
    class OGLGeometry final : public IGeometry {
    public:
        std::vector<PrimitiveImpl>        m_meshes;
        std::vector<GLBuffer>             m_buffers;
        GeometryDesc                      m_desc{};
        std::atomic<bool>                 m_loaded{false};
        std::shared_ptr<OGLDeletionQueue> m_deletion_queue;

        OGLGeometry() = default;
        OKAMI_NO_COPY(OGLGeometry);
        ~OGLGeometry();

        GeometryDesc const& GetDesc()  const override { return m_desc; }
        bool                IsLoaded() const override {
            return m_loaded.load(std::memory_order_acquire);
        }
    };

    // The single OGL geometry manager.
    //
    // Geometry is NOT stored in the entt registry.  Lifetime is managed
    // entirely through shared_ptr (GeometryHandle).  Async file loading uses
    // the existing IO-thread signal machinery.
    class OGLGeometryManager final :
        public EngineModule,
        public IGeometryManager {
    private:
        struct PendingLoad {
            std::shared_ptr<OGLGeometry> m_geometry; // pre-created handle
        };

        std::shared_ptr<OGLDeletionQueue> m_deletion_queue =
            std::make_shared<OGLDeletionQueue>();

        std::mutex m_mtx;
        std::unordered_map<std::filesystem::path, std::weak_ptr<OGLGeometry>, PathHash>
            m_path_cache;
        std::unordered_map<uint32_t, std::unique_ptr<PendingLoad>>
            m_pending;
        std::atomic<uint32_t> m_next_id{1};

        DefaultSignalHandler<OnResourceLoadedEvent<Geometry>> m_loaded_handler;

        Error RegisterImpl(InterfaceCollection& ic) override;
        Error ReceiveMessagesImpl(MessageBus& bus, RecieveMessagesParams const& params) override;

    public:
        // IGeometryManager
        GeometryHandle LoadGeometry(
            std::filesystem::path const& path,
            GeometryLoadParams           params,
            InterfaceCollection&         ic) override;
        GeometryHandle CreateGeometry(Geometry data) override;

        // Convenience downcast – valid for any GeometryHandle produced by this manager.
        static OGLGeometry* GetOGLGeometry(GeometryHandle const& handle) {
            return static_cast<OGLGeometry*>(handle.get());
        }

        std::string GetName() const override { return "OGL Geometry Manager"; }
    };
}
