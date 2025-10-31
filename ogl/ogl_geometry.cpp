#include "ogl_geometry.hpp"

#include "shaders/common.glsl"

using namespace okami;

Expected<std::pair<typename Geometry::Desc, GeometryImpl>> 
    OGLGeometryManager::CreateResource(Geometry&& data, std::any userData) {

    // Upload buffers to OpenGL
    std::vector<GLBuffer> oglBuffers;
    for (auto const& bufferData : data.GetBuffers()) {
        Error err;

        GLBuffer oglBuffer;
        glGenBuffers(1, oglBuffer.ptr()); err += GET_GL_ERROR();
        glBindBuffer(GL_ARRAY_BUFFER, oglBuffer.get()); err += GET_GL_ERROR();
        glBufferData(GL_ARRAY_BUFFER, bufferData.size(), bufferData.data(), GL_STATIC_DRAW); err += GET_GL_ERROR();

        OKAMI_UNEXPECTED_RETURN_IF(!oglBuffer, "Failed to create OpenGL buffer for geometry");
        OKAMI_UNEXPECTED_RETURN(err);
    }

    // Build VAOs for each mesh
    std::vector<MeshImpl> meshesImpl;
    for (auto const& mesh : data.GetMeshes()) {
        auto vsInputMetaFunc = [&]() {
            switch (mesh.m_type) {
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
            auto attribute = mesh.TryGetAttribute(attribInfo.type);
            OKAMI_UNEXPECTED_RETURN_IF(!attribute, 
                "Mesh is missing required attribute for location " + std::to_string(location));

            setupByLocation[location] = glsl::VertexArraySetupAttrib{
                .buffer = oglBuffers[attribute->m_buffer].get(),
                .offset = attribute->m_offset
            };
        }
        
        // Get index buffer if present
        std::optional<glsl::VertexArraySetupAttrib> indexBufferSetup;
        if (mesh.HasIndexBuffer()) {
            auto const& indexInfo = mesh.m_indices.value();
            indexBufferSetup = glsl::VertexArraySetupAttrib{
                .buffer = oglBuffers[indexInfo.m_buffer].get(),
                .offset = indexInfo.m_offset
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

        meshesImpl.emplace_back(MeshImpl{
            .m_vao = std::move(vao),
            .m_indexBufferIndex = mesh.HasIndexBuffer() ? std::optional<int>{ mesh.m_indices->m_buffer } : std::nullopt,
            .m_indexBufferOffset = mesh.HasIndexBuffer() ? mesh.m_indices->m_offset : 0,
        });
    }

    return std::make_pair(
        data.GetDesc(),
        GeometryImpl{
            .m_meshes = std::move(meshesImpl),
            .m_buffers = std::move(oglBuffers),
        }
    );
}

void OGLGeometryManager::SetStaticMeshMetaFunc(glsl::vs_meta_func_t func) {
    m_staticMeshMetaFunc = func;
}