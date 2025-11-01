#include "ogl_geometry.hpp"

#include "shaders/common.glsl"

#include <glog/logging.h>   

using namespace okami;

Expected<std::pair<typename Geometry::Desc, GeometryImpl>> 
    OGLGeometryManager::CreateResource(Geometry&& data, std::any userData) {

    // Upload buffers to OpenGL
    std::vector<std::shared_ptr<GLBuffer>> oglBuffers;
    for (auto const& bufferData : data.GetBuffers()) {
        Error err;

        GLBuffer oglBuffer;
        glGenBuffers(1, oglBuffer.ptr()); err += GET_GL_ERROR();
        glBindBuffer(GL_ARRAY_BUFFER, oglBuffer.get()); err += GET_GL_ERROR();
        glBufferData(GL_ARRAY_BUFFER, bufferData.size(), bufferData.data(), GL_STATIC_DRAW); err += GET_GL_ERROR();

        OKAMI_UNEXPECTED_RETURN_IF(!oglBuffer, "Failed to create OpenGL buffer for geometry");
        OKAMI_UNEXPECTED_RETURN(err);

        oglBuffers.emplace_back(std::make_shared<GLBuffer>(std::move(oglBuffer)));
    }

    // Build VAOs for each mesh
    std::vector<PrimitiveImpl> primitivesImpl;
    for (auto const& primitive : data.GetPrimitives()) {
        auto vsInputMetaFunc = [&]() {
            switch (primitive.m_type) {
                case okami::MeshType::Static:
                    return m_staticMeshMetaFunc;
                default:
                    return glsl::vs_meta_func_t{};
            }
        }();

        auto inputInfo = glsl::GetVSShaderInputInfo(vsInputMetaFunc);

        std::unordered_map<int, glsl::VertexArraySetupAttrib> setupByLocation;

        // Get vertex attributes    
        for (auto const& [location, attribInfo] : inputInfo.locationToAttrib) {
            // Verify that the mesh has the required attribute
            auto attribute = primitive.TryGetAttribute(attribInfo.type);

            if (!attribute) {
                auto result = OKAMI_UNEXPECTED("Mesh is missing required attribute for attribute " + std::string(AttributeTypeToString(attribInfo.type)) +
                    " at location " + std::to_string(location));
                LOG(ERROR) << result.error();
                return (result);
            }

            OKAMI_UNEXPECTED_RETURN_IF(!attribute, 
                "Mesh is missing required attribute for attribute " + std::string(AttributeTypeToString(attribInfo.type)) +
                " at location " + std::to_string(location));

            setupByLocation[location] = glsl::VertexArraySetupAttrib{
                .buffer = oglBuffers[attribute->m_buffer]->get(),
                .offset = attribute->m_offset,
                .stride = static_cast<GLsizei>(attribute->m_stride)
            };
        }
        
        // Get index buffer if present
        std::optional<glsl::VertexArraySetupAttrib> indexBufferSetup;
        if (primitive.HasIndexBuffer()) {
            auto const& indexInfo = primitive.m_indices.value();
            indexBufferSetup = glsl::VertexArraySetupAttrib{
                .buffer = oglBuffers[indexInfo.m_buffer]->get(),
                .offset = indexInfo.m_offset,
                .stride = static_cast<GLsizei>(indexInfo.GetStride())
            };
        }

        // Create VAO
        GLVertexArray vao;
        glGenVertexArrays(1, vao.ptr()); OKAMI_UNEXPECTED_RETURN_IF(!vao, "Failed to create VAO for geometry mesh");

        glsl::SetupVertexArray(vsInputMetaFunc, glsl::VertexArraySetupArgs{
            .vertexArray = vao.get(),
            .indexBufferSetup = indexBufferSetup,
            .setupByLocation = setupByLocation,
        });
        Error err = GET_GL_ERROR();

        primitivesImpl.emplace_back(PrimitiveImpl{
            .m_vao = std::move(vao),
            .m_indexBufferIndex = primitive.HasIndexBuffer() ? std::optional<int>{ primitive.m_indices->m_buffer } : std::nullopt,
            .m_indexBufferOffset = primitive.HasIndexBuffer() ? primitive.m_indices->m_offset : 0,
        });
    }

    return std::make_pair(
        data.GetDesc(),
        GeometryImpl{
            .m_meshes = std::move(primitivesImpl),
            .m_buffers = std::move(oglBuffers),
        }
    );
}

void OGLGeometryManager::SetStaticMeshMetaFunc(glsl::vs_meta_func_t func) {
    m_staticMeshMetaFunc = func;
}