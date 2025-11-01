#include "ogl_static_mesh.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLStaticMeshRenderer::RegisterImpl(ModuleInterface& mi) {
    m_geometryManager->SetStaticMeshMetaFunc(glsl::__create_vertex_array_StaticMeshVertex);

    return {};
}

Error OGLStaticMeshRenderer::StartupImpl(ModuleInterface& mi) {
    auto* cache = mi.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLSpriteRenderer");

    m_transformView = mi.m_interfaces.Query<IComponentView<Transform>>();
    OKAMI_ERROR_RETURN_IF(!m_transformView, "IComponentView<Transform> interface not available for OGLSpriteRenderer");

    // Create shader program with vertex, geometry, and fragment shaders
    auto program = CreateProgram(ProgramShaderPaths{
        .m_vertex = GetGLSLShaderPath("static_mesh.vs"),
        .m_fragment = GetGLSLShaderPath("static_mesh.fs"),
    }, *cache);
    OKAMI_ERROR_RETURN(program);

    m_program = std::move(*program);

    auto instanceUBO = UniformBuffer<glsl::StaticMeshInstance>::Create(m_program, 
        "StaticMeshInstanceBlock", 0);
    OKAMI_ERROR_RETURN(instanceUBO);
    m_instanceUBO = std::move(*instanceUBO);

    auto sceneUBO = UniformBuffer<glsl::SceneGlobals>::Create(m_program, 
        "SceneGlobalsBlock", 1);
    OKAMI_ERROR_RETURN(sceneUBO);
    m_sceneUBO = std::move(*sceneUBO);

    LOG(INFO) << "OGL Static Mesh Renderer initialized successfully";
    return {};
}

void OGLStaticMeshRenderer::ShutdownImpl(ModuleInterface& mi) {

}

Error OGLStaticMeshRenderer::ProcessFrameImpl(Time const&, ModuleInterface&) {
    return {};
}

Error OGLStaticMeshRenderer::MergeImpl() {
    return {};
}

OGLStaticMeshRenderer::OGLStaticMeshRenderer(OGLGeometryManager* geometryManager) :
    m_geometryManager(geometryManager) {
    m_storage = CreateChild<StorageModule<StaticMeshComponent>>();
}

Error OGLStaticMeshRenderer::Pass(OGLPass const& pass) {
    // Collect all sprites and their transforms
    std::vector<std::pair<ResHandle<Geometry>, glsl::StaticMeshInstance>> instances;
    
    m_storage->ForEach([&](entity_t entity, const StaticMeshComponent& mesh) {
        // Skip sprites without textures
        if (!mesh.m_geometry || !mesh.m_geometry.IsLoaded()) {
            return;
        }
        
        Transform transform = m_transformView->GetOr(entity, Transform::Identity());

        instances.emplace_back(std::make_pair(mesh.m_geometry, glsl::StaticMeshInstance{
            .u_model = transform.AsMatrix(),
            .u_normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform.AsMatrix()))),
        }));
    });

    // Sort sprites back-to-front for proper alpha blending
    std::sort(instances.begin(), instances.end(),
        [this](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    // Set up rendering state
    glUseProgram(m_program.get()); 
    OKAMI_DEFER(glUseProgram(0)); OKAMI_CHK_GL;

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    m_instanceUBO.Bind();
    OKAMI_DEFER(m_instanceUBO.Unbind());

    m_sceneUBO.Bind();
    OKAMI_DEFER(m_sceneUBO.Unbind()); OKAMI_CHK_GL;
    m_sceneUBO.Write(pass.m_sceneGlobals); OKAMI_CHK_GL;

    OKAMI_DEFER(glBindVertexArray(0));
    
    // Group sprites by texture to minimize texture binding
    uint32_t batchStart = 0;

    okami::MeshImpl* currentMeshImpl = nullptr;
    ResHandle<Geometry> currentGeometry;

    if (instances.empty()) {
        return {};
    }
    
    for (auto const& geo : instances) {
        ResHandle<Geometry> geometry = geo.first;
        
        // Check if we need to flush the current batch
        if (geometry != currentGeometry) {
            if (geometry.IsLoaded()) {
                currentMeshImpl = &m_geometryManager->GetImpl(geometry)->m_meshes[0];
                glBindVertexArray(currentMeshImpl->m_vao.get());
                OKAMI_CHK_GL;
            } else {
                currentMeshImpl = nullptr;
            }
        
            currentGeometry = geometry;
        }

        if (currentMeshImpl) {
            m_instanceUBO.Write(geo.second);
            auto const& desc = currentGeometry.GetDesc();
            
            if (desc.m_meshes[0].m_indices) {
                glDrawElements(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(desc.m_meshes[0].m_indices->m_count),
                    GL_UNSIGNED_INT,
                    reinterpret_cast<void*>(currentMeshImpl->m_indexBufferOffset)
                ); OKAMI_CHK_GL;
            } else {
                glDrawArrays(
                    GL_TRIANGLES,
                    0,
                    static_cast<GLsizei>(desc.m_meshes[0].m_vertexCount)
                ); OKAMI_CHK_GL;
            }
        }
    }
    
    return {};
}

std::string OGLStaticMeshRenderer::GetName() const {
    return "OGL Static Mesh Renderer";
}