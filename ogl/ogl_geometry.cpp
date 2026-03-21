#include "ogl_geometry.hpp"

#include "shaders/common.glsl"
#include "shaders/static_mesh.glsl"
#include "shaders/skinned_mesh.glsl"

#include <glog/logging.h>

using namespace okami;

// ---------------------------------------------------------------------------
// OGLGeometry destructor – defers GL deletions to the GL thread
// ---------------------------------------------------------------------------

OGLGeometry::~OGLGeometry() {
    if (m_deletion_queue) {
        for (auto& prim : m_meshes) {
            if (GLuint id = prim.m_vao.release(); id != 0) {
                m_deletion_queue->PushVAO(id);
            }
        }
        for (auto& buf : m_buffers) {
            if (GLuint id = buf.release(); id != 0) {
                m_deletion_queue->PushBuffer(id);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: build GL buffers/VAOs from CPU-side Geometry (GL thread only)
// ---------------------------------------------------------------------------

static Error UploadToGL(OGLGeometry& out, Geometry&& data) {
    std::vector<GLBuffer>     oglBuffers;
    std::vector<PrimitiveImpl> primitivesImpl;
    GeometryDesc newDesc;

    for (auto const& primitive : data.GetPrimitives()) {
        auto vsInputInfo = [&]() -> glsl::VertexShaderInputInfo {
            switch (primitive.m_type) {
                case okami::MeshType::Static:
                    return glsl::__get_vs_input_infoStaticMeshVertex();
                case okami::MeshType::Skinned:
                    return glsl::__get_vs_input_infoSkinnedMeshVertex();
                default: return {};
            }
        }();

        std::vector<uint8_t> bufferData;
        size_t totalSize = vsInputInfo.m_totalStride * primitive.m_vertexCount;
        bufferData.resize(totalSize);

        for (auto const& [location, attribInfo] : vsInputInfo.locationToAttrib) {
            auto meshAttribute = primitive.TryGetAttribute(attribInfo.m_type);
            OKAMI_ERROR_RETURN_IF(!meshAttribute,
                "Mesh is missing required attribute: " +
                std::string(AttributeTypeToString(attribInfo.m_type)));
            OKAMI_ERROR_RETURN_IF(attribInfo.m_frequency == glsl::Frequency::PerInstance,
                "Instanced attributes are not supported in this context");

            auto sz      = meshAttribute->GetComponentSize();
            auto srcData = data.GetRawVertexData(meshAttribute->m_buffer).data();
            auto dstData = bufferData.data();
            for (int i = 0; i < static_cast<int>(primitive.m_vertexCount); ++i) {
                size_t srcOffset = meshAttribute->m_offset + i * meshAttribute->GetStride();
                size_t dstOffset = attribInfo.m_offset    + i * vsInputInfo.m_totalStride;
                std::memcpy(dstData + dstOffset, srcData + srcOffset, sz);
            }
        }

        // Vertex buffer
        GLBuffer vertexBuffer;
        glGenBuffers(1, vertexBuffer.ptr());
        OKAMI_ERROR_RETURN_IF(!vertexBuffer, "Failed to create vertex buffer");
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
        OKAMI_DEFER(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glBufferData(GL_ARRAY_BUFFER, totalSize, bufferData.data(), GL_STATIC_DRAW);
        int vertexBufferIndex = static_cast<int>(oglBuffers.size());
        oglBuffers.push_back(std::move(vertexBuffer));

        // Index buffer (optional)
        std::optional<int> indexBufferIndex;
        if (primitive.HasIndexBuffer()) {
            GLBuffer indexBuffer;
            glGenBuffers(1, indexBuffer.ptr());
            OKAMI_ERROR_RETURN_IF(!indexBuffer, "Failed to create index buffer");
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());
            OKAMI_DEFER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
            auto const& rawBuf = data.GetBuffers()[primitive.m_indices->m_buffer];
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                primitive.m_indices->GetTotalSize(),
                rawBuf.data() + primitive.m_indices->m_offset,
                GL_STATIC_DRAW);
            indexBufferIndex = static_cast<int>(oglBuffers.size());
            oglBuffers.push_back(std::move(indexBuffer));
        }

        // VAO
        GLVertexArray vao;
        glGenVertexArrays(1, vao.ptr());
        OKAMI_ERROR_RETURN_IF(!vao, "Failed to create VAO");
        SetupVertexArray(vao, vsInputInfo,
            oglBuffers[vertexBufferIndex].get(),
            indexBufferIndex
                ? std::make_optional(oglBuffers[*indexBufferIndex].get())
                : std::nullopt);
        OKAMI_ERROR_RETURN(GET_GL_ERROR());

        primitivesImpl.emplace_back(PrimitiveImpl{.m_vao = std::move(vao)});

        GeometryPrimitiveDesc newPrim = primitive;
        for (auto& [_, attr] : newPrim.m_attributes) {
            attr.m_buffer = vertexBufferIndex;
            attr.m_offset = 0;
            attr.m_stride = vsInputInfo.m_totalStride;
        }
        if (newPrim.m_indices) {
            newPrim.m_indices->m_buffer = *indexBufferIndex;
            newPrim.m_indices->m_offset = 0;
        }
        newDesc.m_primitives.push_back(std::move(newPrim));
    }

    out.m_meshes  = std::move(primitivesImpl);
    out.m_buffers = std::move(oglBuffers);
    out.m_desc    = std::move(newDesc);
    out.m_loaded.store(true, std::memory_order_release);
    return {};
}

// ---------------------------------------------------------------------------
// OGLGeometryManager
// ---------------------------------------------------------------------------

Error OGLGeometryManager::RegisterImpl(InterfaceCollection& ic) {
    ic.Register<IGeometryManager>(this);
    ic.RegisterSignalHandler<OnResourceLoadedEvent<Geometry>>(&m_loaded_handler);
    return {};
}

GeometryHandle OGLGeometryManager::LoadGeometry(
    std::filesystem::path const& path,
    GeometryLoadParams            params,
    InterfaceCollection&          ic) {

    // Dedup: return existing handle if still alive.
    {
        std::lock_guard lock(m_mtx);
        auto it = m_path_cache.find(path);
        if (it != m_path_cache.end()) {
            if (auto existing = it->second.lock()) {
                return existing;
            }
            m_path_cache.erase(it);
        }
    }

    auto id      = m_next_id.fetch_add(1, std::memory_order_relaxed);
    auto pending = std::make_unique<PendingLoad>();
    pending->m_geometry                   = std::make_shared<OGLGeometry>();
    pending->m_geometry->m_deletion_queue = m_deletion_queue;

    GeometryHandle handle = pending->m_geometry;

    {
        std::lock_guard lock(m_mtx);
        m_path_cache[path] = pending->m_geometry;
        m_pending[id]      = std::move(pending);
    }

    ic.SendSignal(LoadResourceSignal<Geometry>{
        .m_path   = path,
        .m_params = params,
        .m_id     = id,
    });

    return handle;
}

GeometryHandle OGLGeometryManager::CreateGeometry(Geometry data) {
    auto geo = std::make_shared<OGLGeometry>();
    geo->m_deletion_queue = m_deletion_queue;
    auto err = UploadToGL(*geo, std::move(data));
    if (err.IsError()) {
        LOG(WARNING) << "OGLGeometryManager::CreateGeometry: GL upload failed: " << err;
    }
    return geo;
}

Error OGLGeometryManager::ReceiveMessagesImpl(
    MessageBus& /*bus*/, RecieveMessagesParams const& /*params*/) {

    // Drain GL deletions deferred from non-GL threads.
    m_deletion_queue->Drain();

    Error err;

    m_loaded_handler.Handle([&](OnResourceLoadedEvent<Geometry> msg) {
        if (!msg.m_data) {
            err += msg.m_data.error();
            LOG(ERROR) << "OGLGeometryManager: geometry load failed: " << msg.m_data.error();
            return;
        }

        auto id = msg.m_id;

        std::shared_ptr<OGLGeometry> geo;
        {
            std::lock_guard lock(m_mtx);
            auto it = m_pending.find(id);
            if (it == m_pending.end()) {
                return; // handle was released before load completed
            }
            geo = it->second->m_geometry;
            m_pending.erase(it);
        }

        err += UploadToGL(*geo, std::move(*msg.m_data));
    });

    return err;
}


