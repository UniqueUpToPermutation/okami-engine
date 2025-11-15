#include "ogl_geometry.hpp"

#include "shaders/common.glsl"
#include "shaders/static_mesh.glsl"

#include <glog/logging.h>   

using namespace okami;

Expected<std::pair<typename Geometry::Desc, GeometryImpl>> 
    OGLGeometryManager::CreateResource(Geometry&& data, std::any userData) {

    std::vector<std::shared_ptr<GLBuffer>> oglBuffers;

    // Build VAOs for each mesh
    std::vector<PrimitiveImpl> primitivesImpl;
    GeometryDesc newDesc;
    for (auto const& primitive : data.GetPrimitives()) {
        auto vsInputInfo = [&]() -> glsl::VertexShaderInputInfo {
            switch (primitive.m_type) {
                case okami::MeshType::Static:
                    return glsl::__get_vs_input_infoStaticMeshVertex();
                default:
                    return {};
            }
        }();

        std::vector<uint8_t> bufferData;

        size_t totalSize = vsInputInfo.m_totalStride * primitive.m_vertexCount;
        bufferData.resize(totalSize);
       
        // Copy attribute data into temporary buffer and then to OpenGL buffer
        // Interleave attributes as per vsInputInfo
        for (auto const& [location, attribInfo] : vsInputInfo.locationToAttrib) {
            // Verify that the mesh has the required attribute
            auto meshAttribute = primitive.TryGetAttribute(attribInfo.m_type);

            if (!meshAttribute) {
                auto result = OKAMI_UNEXPECTED("Mesh is missing required attribute for attribute " + 
                    std::string(AttributeTypeToString(attribInfo.m_type)) +
                    " at location " + std::to_string(location));
                LOG(ERROR) << result.error();
                return (result);
            }

            OKAMI_UNEXPECTED_RETURN_IF(!meshAttribute, 
                "Mesh is missing required attribute for attribute " + 
                std::string(AttributeTypeToString(attribInfo.m_type)) +
                " at location " + std::to_string(location));

            OKAMI_UNEXPECTED_RETURN_IF(attribInfo.m_frequency == glsl::Frequency::PerInstance,
                "Instanced attributes are not supported in this context");

            // Copy attribute data into temp buffer
            auto sz = meshAttribute->GetComponentSize();
            auto srcData = data.GetRawVertexData(meshAttribute->m_buffer).data();
            auto destData = bufferData.data();
            for (int i = 0; i < primitive.m_vertexCount; ++i) {
                size_t srcOffset = meshAttribute->m_offset + i * meshAttribute->GetStride();
                size_t dstOffset = attribInfo.m_offset + i * vsInputInfo.m_totalStride;
                std::memcpy(destData + dstOffset, srcData + srcOffset, sz);
            } 
        }

        // Create vertex buffer and upload data
        GLBuffer vertexBuffer;
        glGenBuffers(1, vertexBuffer.ptr());
        OKAMI_UNEXPECTED_RETURN_IF(!vertexBuffer, "Failed to create OpenGL buffer for geometry mesh");
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get()); OKAMI_DEFER(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glBufferData(GL_ARRAY_BUFFER, totalSize, bufferData.data(), GL_STATIC_DRAW);
        int vertexBufferIndex = static_cast<int>(oglBuffers.size());
        oglBuffers.push_back(std::make_shared<GLBuffer>(std::move(vertexBuffer)));

        // Create index buffer if needed
        std::optional<GLBuffer> indexBuffer;
        std::optional<int> indexBufferIndex;
        if (primitive.HasIndexBuffer()) {
            indexBuffer.emplace();
            glGenBuffers(1, indexBuffer->ptr());
            OKAMI_UNEXPECTED_RETURN_IF(!*indexBuffer, "Failed to create OpenGL index buffer for geometry mesh");
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->get()); OKAMI_DEFER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
            auto& buffer = data.GetBuffers()[primitive.m_indices->m_buffer];

            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                primitive.m_indices->GetTotalSize(), 
                buffer.data() + primitive.m_indices->m_offset, GL_STATIC_DRAW);
            indexBufferIndex = static_cast<int>(oglBuffers.size());
            oglBuffers.push_back(std::make_shared<GLBuffer>(std::move(*indexBuffer)));
        }

        // Create VAO
        GLVertexArray vao;
        glGenVertexArrays(1, vao.ptr()); OKAMI_UNEXPECTED_RETURN_IF(!vao, "Failed to create VAO for geometry mesh");

        SetupVertexArray(vao, vsInputInfo, oglBuffers[vertexBufferIndex]->get(), 
            indexBufferIndex ? std::make_optional(oglBuffers[indexBufferIndex.value()]->get()) : std::nullopt);
        Error err = GET_GL_ERROR();

        primitivesImpl.emplace_back(PrimitiveImpl{.m_vao = std::move(vao)});

        GeometryPrimitiveDesc newPrimitiveDesc = primitive;

        for (auto it = newPrimitiveDesc.m_attributes.begin(); 
            it != newPrimitiveDesc.m_attributes.end(); ++it) {
            it->second.m_buffer = vertexBufferIndex;
            it->second.m_offset = 0;
            it->second.m_stride = vsInputInfo.m_totalStride;
        }

        if (newPrimitiveDesc.m_indices) {
            newPrimitiveDesc.m_indices->m_buffer = indexBufferIndex.value();
            newPrimitiveDesc.m_indices->m_offset = 0;
        }

        newDesc.m_primitives.push_back(std::move(newPrimitiveDesc));
    }

    return std::make_pair(
        newDesc,
        GeometryImpl{
            .m_meshes = std::move(primitivesImpl),
            .m_buffers = std::move(oglBuffers),
        }
    );
}

void OGLGeometryManager::SetStaticMeshVSInput(glsl::VertexShaderInputInfo info) {
    m_staticMeshVSInput = info;
}