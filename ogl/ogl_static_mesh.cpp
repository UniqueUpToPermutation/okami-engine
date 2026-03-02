#include "ogl_static_mesh.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLStaticMeshRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    m_geometryManager->SetStaticMeshVSInput(glsl::__get_vs_input_infoStaticMeshVertex());
    return {};
}

Error OGLStaticMeshRenderer::StartupImpl(InitContext const& context) {
    Error err;

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider,
        "IOGLSceneGlobalsProvider interface not available for OGLStaticMeshRenderer");

    // Obtain the default material (DefaultMaterial) from the material manager.
    auto* matMgr = context.m_interfaces.Query<IMaterialManager<DefaultMaterial>>();
    OKAMI_ERROR_RETURN_IF(!matMgr,
        "IMaterialManager<DefaultMaterial> not available for OGLStaticMeshRenderer");
    m_defaultMaterial = matMgr->CreateMaterial(DefaultMaterial{});

    auto instanceUBO = UniformBuffer<glsl::StaticMeshInstance>::Create();
    OKAMI_ERROR_RETURN(instanceUBO);
    m_instanceUBO = std::move(*instanceUBO);

    m_pipelineState.depthTestEnabled = true;
    m_pipelineState.blendEnabled     = false;
    m_pipelineState.cullFaceEnabled  = true;
    m_pipelineState.depthMask        = true;

    LOG(INFO) << "OGL Static Mesh Renderer initialized successfully";
    return err;
}

OGLStaticMeshRenderer::OGLStaticMeshRenderer(OGLGeometryManager* geometryManager)
    : m_geometryManager(geometryManager) {}

Error OGLStaticMeshRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    struct InstanceData {
        ResHandle<Geometry>              m_geometry;
        MaterialHandle                   m_material;
        glsl::StaticMeshInstance         m_glslData;
    };

    std::vector<InstanceData> instances;

    registry.view<StaticMeshComponent, Transform>().each(
        [&](auto entity, StaticMeshComponent const& mesh, Transform const& transform) {
            if (!mesh.m_geometry || !mesh.m_geometry.IsLoaded()) {
                return;
            }
            auto matrix = transform.AsMatrix();
            instances.emplace_back(InstanceData{
                .m_geometry = mesh.m_geometry,
                .m_material = mesh.m_material,
                .m_glslData = glsl::StaticMeshInstance{
                    .u_model        = matrix,
                    .u_normalMatrix = glm::transpose(glm::inverse(matrix)),
                }
            });
        });

    if (instances.empty()) {
        return {};
    }

    Error err;

    // Sort by (geometry entity, material pointer) to minimise state changes.
    std::sort(instances.begin(), instances.end(),
        [](InstanceData const& a, InstanceData const& b) {
            if (a.m_geometry == b.m_geometry) {
                return a.m_material.get() < b.m_material.get();
            }
            return a.m_geometry.GetEntity() < b.m_geometry.GetEntity();
        });

    m_pipelineState.SetToGL();
    err += GET_GL_ERROR();
    err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);

    PrimitiveImpl*      currentMeshImpl = nullptr;
    ResHandle<Geometry> currentGeometry;
    OGLMaterial*        currentMat      = nullptr;

    for (auto const& inst : instances) {
        if (!inst.m_geometry) {
            continue;
        }

        // Resolve effective material (fall back to default when none set).
        auto* mat = static_cast<OGLMaterial*>(
            inst.m_material ? inst.m_material.get() : m_defaultMaterial.get());

        // Geometry state change
        if (inst.m_geometry != currentGeometry) {
            currentMeshImpl = &m_geometryManager->GetImpl(inst.m_geometry)->m_meshes[0];
            glBindVertexArray(currentMeshImpl->m_vao.get());
            err += GET_GL_ERROR();
            currentGeometry = inst.m_geometry;
        }

        // Material state change
        if (mat != currentMat) {
            if (mat) {
                mat->Bind();
                err += GET_GL_ERROR();
                err += m_instanceUBO.Bind(BufferBindingPoints::StaticMeshInstance);
                err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);
            }
            currentMat = mat;
        }

        // Draw
        if (currentMeshImpl && mat) {
            err += m_instanceUBO.Write(inst.m_glslData);
            auto const& desc  = currentGeometry.GetDesc();
            auto const& prim  = desc.m_primitives[0];

            if (prim.m_indices) {
                GLint indexType = ToOpenGL(prim.m_indices->m_type);
                glDrawElements(GL_TRIANGLES,
                    static_cast<GLsizei>(prim.m_indices->m_count),
                    indexType,
                    (void*)prim.m_indices->m_offset);
            } else {
                glDrawArrays(GL_TRIANGLES, 0,
                    static_cast<GLsizei>(prim.m_vertexCount));
            }
            err += GET_GL_ERROR();
        }
    }

    return err;
}

std::string OGLStaticMeshRenderer::GetName() const {
    return "OGL Static Mesh Renderer";
}
