#include "ogl_static_mesh.hpp"

#include "../paths.hpp"
#include <glog/logging.h>

using namespace okami;

Error OGLStaticMeshRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    return {};
}

Error OGLStaticMeshRenderer::StartupImpl(InitContext const& context) {
    Error err;

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider,
        "IOGLSceneGlobalsProvider interface not available for OGLStaticMeshRenderer");

    m_depthPassProvider = context.m_interfaces.Query<IOGLDepthPassProvider>();
    OKAMI_ERROR_RETURN_IF(!m_depthPassProvider,
        "IOGLDepthPassProvider interface not available for OGLStaticMeshRenderer");

    // Obtain the default material (DefaultMaterial) from the material manager.
    auto* matMgr = context.m_interfaces.Query<IMaterialManager<DefaultMaterial>>();
    OKAMI_ERROR_RETURN_IF(!matMgr,
        "IMaterialManager<DefaultMaterial> not available for OGLStaticMeshRenderer");
    m_defaultMaterial = matMgr->CreateMaterial(DefaultMaterial{});

    auto instanceVBO = UploadVertexBuffer<glsl::StaticMeshInstance>::Create(
        glsl::__get_vs_input_infoStaticMeshInstance(), 1);
    OKAMI_ERROR_RETURN(instanceVBO);
    m_instanceVBO = std::move(*instanceVBO);

    m_pipelineState.depthTestEnabled = true;
    m_pipelineState.blendEnabled     = false;
    m_pipelineState.cullFaceEnabled  = true;
    m_pipelineState.depthMask        = true;

    // Compile the depth-only program used for shadow passes.
    {
        auto* cache = context.m_interfaces.Query<IGLShaderCache>();
        OKAMI_ERROR_RETURN_IF(!cache, "OGLStaticMeshRenderer: IGLShaderCache not available");
        ProgramShaderPaths depthPaths;
        depthPaths.m_vertex   = GetGLSLShaderPath("static_mesh_depth.vs");
        depthPaths.m_geometry = GetGLSLShaderPath("static_mesh_depth.gs");
        depthPaths.m_fragment = GetGLSLShaderPath("static_mesh_depth.fs");
        auto depthProg = CreateProgram(depthPaths, *cache);
        if (!depthProg) {
            LOG(ERROR) << "OGLStaticMeshRenderer: Failed to compile depth program: " << depthProg.error();
            err += depthProg.error();
        } else {
            m_depthProgram = std::move(*depthProg);
            glUseProgram(m_depthProgram.get());
            err += AssignBufferBindingPoint(m_depthProgram, "CascadeBlock", 0);
            glUseProgram(0);
        }
        OKAMI_ERROR_RETURN(err);
    }

    LOG(INFO) << "OGL Static Mesh Renderer initialized successfully";
    return err;
}

OGLStaticMeshRenderer::OGLStaticMeshRenderer(OGLGeometryManager* geometryManager)
    : m_geometryManager(geometryManager) {}

Error OGLStaticMeshRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    struct InstanceData {
        GeometryHandle                   m_geometry;
        MaterialHandle                   m_material;
        glsl::StaticMeshInstance         m_glslData;
    };

    std::vector<InstanceData> instances;

    registry.view<StaticMeshComponent, Transform>().each(
        [&](auto entity, StaticMeshComponent const& mesh, Transform const& transform) {
            if (!mesh.m_geometry || !mesh.m_geometry->IsLoaded()) {
                return;
            }
            auto matrix       = transform.AsMatrix();
            auto normalMatrix  = glm::transpose(glm::inverse(matrix));
            instances.emplace_back(InstanceData{
                .m_geometry = mesh.m_geometry,
                .m_material = mesh.m_material,
                .m_glslData = glsl::StaticMeshInstance{
                    .a_instanceModel_col0   = matrix[0],
                    .a_instanceModel_col1   = matrix[1],
                    .a_instanceModel_col2   = matrix[2],
                    .a_instanceModel_col3   = matrix[3],
                    .a_instanceNormal_col0  = normalMatrix[0],
                    .a_instanceNormal_col1  = normalMatrix[1],
                    .a_instanceNormal_col2  = normalMatrix[2],
                    .a_instanceNormal_col3  = normalMatrix[3],
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
            return a.m_geometry.get() < b.m_geometry.get();
        });

    // Upload all per-instance data to the instance VBO, resizing if needed.
    const size_t instanceCount = instances.size();

    err += m_instanceVBO.Reserve(instanceCount);
    OKAMI_ERROR_RETURN(err);
    {
        auto map = m_instanceVBO.Map();
        OKAMI_ERROR_RETURN_IF(!map, "Failed to map instance VBO");
        for (size_t i = 0; i < instanceCount; ++i) {
            (*map)[i] = instances[i].m_glslData;
        }
    }
    err += GET_GL_ERROR();

    m_pipelineState.SetToGL();
    err += GET_GL_ERROR();

    // For forward passes, bind the shadow map array texture once for all draw groups.
    if (pass.m_type != OGLPassType::Shadow) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthPassProvider->GetDepthTexture());
        err += GET_GL_ERROR();
    }

    // Draw one instanced call per (geometry, material) group.
    size_t groupStart = 0;
    while (groupStart < instanceCount) {
        // Find end of this group.
        size_t groupEnd = groupStart + 1;
        while (groupEnd < instanceCount &&
               instances[groupEnd].m_geometry == instances[groupStart].m_geometry &&
               instances[groupEnd].m_material == instances[groupStart].m_material) {
            ++groupEnd;
        }
        const size_t groupSize = groupEnd - groupStart;

        auto const& inst0    = instances[groupStart];
        auto*       meshImpl = &OGLGeometryManager::GetOGLGeometry(inst0.m_geometry)->m_meshes[0];
        auto*       mat      = static_cast<OGLMaterial*>(
            inst0.m_material ? inst0.m_material.get() : m_defaultMaterial.get());

        if (!mat) {
            groupStart = groupEnd;
            continue;
        }

        // Bind geometry VAO then attach the per-instance attributes from the instance VBO.
        glBindVertexArray(meshImpl->m_vao.get());
        err += GET_GL_ERROR();

        auto const instInfo = glsl::__get_vs_input_infoStaticMeshInstance();
        const GLsizei    instanceStride = static_cast<GLsizei>(instInfo.m_totalStride);
        const GLsizeiptr groupOffset    = static_cast<GLsizeiptr>(groupStart) * instanceStride;

        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO.GetBuffer());
        for (auto const& [location, attrib] : instInfo.locationToAttrib) {
            const GLuint loc = static_cast<GLuint>(location);
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(
                loc,
                static_cast<GLint>(attrib.m_componentCount),
                ToOpenGL(attrib.m_componentType),
                attrib.m_isNormalized ? GL_TRUE : GL_FALSE,
                instanceStride,
                reinterpret_cast<void*>(groupOffset + static_cast<GLsizeiptr>(attrib.m_offset)));
            glVertexAttribDivisor(loc, 1);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        err += GET_GL_ERROR();

        if (pass.m_type == OGLPassType::Shadow) {
            glUseProgram(m_depthProgram.get());
            err += m_depthPassProvider->GetCascadesBuffer().Bind(0);
        } else {
            mat->Bind();
            err += GET_GL_ERROR();
            err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);
        }

        auto const& desc = inst0.m_geometry->GetDesc();
        auto const& prim = desc.m_primitives[0];
        if (prim.m_indices) {
            glDrawElementsInstanced(
                GL_TRIANGLES,
                static_cast<GLsizei>(prim.m_indices->m_count),
                ToOpenGL(prim.m_indices->m_type),
                reinterpret_cast<void*>(prim.m_indices->m_offset),
                static_cast<GLsizei>(groupSize));
        } else {
            glDrawArraysInstanced(
                GL_TRIANGLES, 0,
                static_cast<GLsizei>(prim.m_vertexCount),
                static_cast<GLsizei>(groupSize));
        }
        err += GET_GL_ERROR();

        groupStart = groupEnd;
    }

    glBindVertexArray(0);
    return err;
}

std::string OGLStaticMeshRenderer::GetName() const {
    return "OGL Static Mesh Renderer";
}
