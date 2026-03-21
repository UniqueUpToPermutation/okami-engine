#include "ogl_skinned_mesh.hpp"

#include "../paths.hpp"
#include "../animation.hpp"

#include <ozz/base/maths/simd_math.h>
#include <glog/logging.h>

using namespace okami;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {
    // Convert an ozz column-major Float4x4 to a glm::mat4.
    glm::mat4 OzzToGlm(ozz::math::Float4x4 const& m) {
        glm::mat4 out;
        ozz::math::StorePtrU(m.cols[0], &out[0][0]);
        ozz::math::StorePtrU(m.cols[1], &out[1][0]);
        ozz::math::StorePtrU(m.cols[2], &out[2][0]);
        ozz::math::StorePtrU(m.cols[3], &out[3][0]);
        return out;
    }
} // namespace

// ---------------------------------------------------------------------------
// OGLSkinnedMeshRenderer::RegisterImpl
// ---------------------------------------------------------------------------

Error OGLSkinnedMeshRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    return {};
}

// ---------------------------------------------------------------------------
// OGLSkinnedMeshRenderer::StartupImpl
// ---------------------------------------------------------------------------

Error OGLSkinnedMeshRenderer::StartupImpl(InitContext const& context) {
    Error err;

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider,
        "IOGLSceneGlobalsProvider interface not available for OGLSkinnedMeshRenderer");

    m_depthPassProvider = context.m_interfaces.Query<IOGLDepthPassProvider>();
    OKAMI_ERROR_RETURN_IF(!m_depthPassProvider,
        "IOGLDepthPassProvider interface not available for OGLSkinnedMeshRenderer");

    auto* matMgr = context.m_interfaces.Query<IMaterialManager<DefaultMaterial>>();
    OKAMI_ERROR_RETURN_IF(!matMgr,
        "IMaterialManager<DefaultMaterial> not available for OGLSkinnedMeshRenderer");
    m_defaultMaterial = matMgr->CreateMaterial(DefaultMaterial{});

    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "OGLSkinnedMeshRenderer: IGLShaderCache not available");

    // Instance VBO (single slot; resized on demand).
    auto instVBO = UploadVertexBuffer<glsl::SkinnedMeshInstance>::Create(
        glsl::__get_vs_input_infoSkinnedMeshInstance(), 1);
    OKAMI_ERROR_RETURN(instVBO);
    m_instanceVBO = std::move(*instVBO);

    // Joint matrices UBO.
    GLuint ubo = 0;
    glGenBuffers(1, &ubo);
    m_jointUBO.reset(ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(kMaxJoints * sizeof(glm::mat4)),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    err += GET_GL_ERROR();
    OKAMI_ERROR_RETURN(err);

    // Forward program: skinned_mesh.vs + lambert.fs
    {
        ProgramShaderPaths paths;
        paths.m_vertex   = GetGLSLShaderPath("skinned_mesh.vs");
        paths.m_fragment = GetGLSLShaderPath("lambert.fs");
        auto prog = CreateProgram(paths, *cache);
        if (!prog) {
            LOG(ERROR) << "OGLSkinnedMeshRenderer: Failed to compile forward program: "
                       << prog.error();
            return prog.error();
        }
        m_skinnedForwardProgram = std::move(*prog);
        glUseProgram(m_skinnedForwardProgram.get());
        err += AssignBufferBindingPoint(m_skinnedForwardProgram, "SceneGlobalsBlock",
                                        static_cast<GLint>(ForwardBindPoints::SceneGlobals));
        err += AssignBufferBindingPoint(m_skinnedForwardProgram, "JointMatricesBlock",
                                        static_cast<GLint>(ForwardBindPoints::JointMatrices));
        err += AssignTextureBindingPoint(m_skinnedForwardProgram, "u_diffuseMap", 0);
        err += AssignTextureBindingPoint(m_skinnedForwardProgram, "u_normalMap",  1);
        err += AssignTextureBindingPoint(m_skinnedForwardProgram, "u_shadowMap",  kShadowMapUnit);
        glUseProgram(0);
        OKAMI_ERROR_RETURN(err);
    }

    // Depth program: skinned_mesh_depth.vs + static_mesh_depth.gs + static_mesh_depth.fs
    {
        ProgramShaderPaths paths;
        paths.m_vertex   = GetGLSLShaderPath("skinned_mesh_depth.vs");
        paths.m_geometry = GetGLSLShaderPath("static_mesh_depth.gs");
        paths.m_fragment = GetGLSLShaderPath("static_mesh_depth.fs");
        auto prog = CreateProgram(paths, *cache);
        if (!prog) {
            LOG(ERROR) << "OGLSkinnedMeshRenderer: Failed to compile depth program: "
                       << prog.error();
            return prog.error();
        }
        m_depthProgram = std::move(*prog);
        glUseProgram(m_depthProgram.get());
        err += AssignBufferBindingPoint(m_depthProgram, "JointMatricesBlock",
                                        static_cast<GLint>(DepthBindPoints::JointMatrices));
        err += AssignBufferBindingPoint(m_depthProgram, "CascadeBlock",
                                        static_cast<GLint>(DepthBindPoints::Cascades));
        glUseProgram(0);
        OKAMI_ERROR_RETURN(err);
    }

    m_pipelineState.depthTestEnabled = true;
    m_pipelineState.blendEnabled     = false;
    m_pipelineState.cullFaceEnabled  = true;
    m_pipelineState.depthMask        = true;

    LOG(INFO) << "OGL Skinned Mesh Renderer initialized successfully";
    return err;
}

// ---------------------------------------------------------------------------
// OGLSkinnedMeshRenderer constructor
// ---------------------------------------------------------------------------

OGLSkinnedMeshRenderer::OGLSkinnedMeshRenderer(OGLGeometryManager* geometryManager)
    : m_geometryManager(geometryManager) {}

// ---------------------------------------------------------------------------
// UploadJointMatrices
// ---------------------------------------------------------------------------

bool OGLSkinnedMeshRenderer::UploadJointMatrices(
    entt::registry const& registry,
    SkinnedMeshComponent const& mesh) const
{
    if (mesh.m_skeletonEntity == kNullEntity) return false;

    auto const* state = registry.try_get<SkeletonStateComponent>(mesh.m_skeletonEntity);
    if (!state || !state->IsReady()) return false;

    auto const* skin = mesh.m_skinData.get();
    if (!skin) return false;

    auto const& modelMats  = state->m_modelMatrices;
    auto const& invBinds   = skin->m_inverseBindMatrices;
    auto const& jointIdx   = skin->m_skeletonJointIndices;

    const int numSkinJoints = static_cast<int>(jointIdx.size());
    const int uploadCount   = std::min(numSkinJoints, kMaxJoints);

    // Compute skinning matrices: M_skin[i] = M_model[skeletonJoint[i]] * M_invBind[i]
    // Using a fixed-size stack buffer avoids heap allocation every draw call.
    static thread_local std::vector<glm::mat4> scratchMatrices;
    scratchMatrices.resize(static_cast<size_t>(uploadCount));

    const int numModelMats = static_cast<int>(modelMats.size());
    for (int i = 0; i < uploadCount; ++i) {
        const int si = (i < static_cast<int>(jointIdx.size())) ? jointIdx[i] : -1;
        glm::mat4 worldMat(1.0f);
        if (si >= 0 && si < numModelMats)
            worldMat = OzzToGlm(modelMats[static_cast<size_t>(si)]);
        const glm::mat4* invBind = (i < static_cast<int>(invBinds.size())) ? &invBinds[i] : nullptr;
        scratchMatrices[i] = invBind ? (worldMat * (*invBind)) : worldMat;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_jointUBO.get());
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    static_cast<GLsizeiptr>(uploadCount) * static_cast<GLsizeiptr>(sizeof(glm::mat4)),
                    scratchMatrices.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return true;
}

// ---------------------------------------------------------------------------
// OGLSkinnedMeshRenderer::Pass
// ---------------------------------------------------------------------------

Error OGLSkinnedMeshRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    Error err;

    auto view = registry.view<SkinnedMeshComponent, Transform>();
    if (view.begin() == view.end()) return {};

    m_pipelineState.SetToGL();
    err += GET_GL_ERROR();

    // Bind shadow map for forward pass.
    if (pass.m_type != OGLPassType::Shadow) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthPassProvider->GetDepthTexture());
        err += GET_GL_ERROR();
    }

    // Bind the joint UBO to its slot (stays bound; contents are updated per draw).
    const GLint jointBindPt = (pass.m_type == OGLPassType::Shadow)
        ? static_cast<GLint>(DepthBindPoints::JointMatrices)
        : static_cast<GLint>(ForwardBindPoints::JointMatrices);

    glBindBufferBase(GL_UNIFORM_BUFFER,
                     static_cast<GLuint>(jointBindPt),
                     m_jointUBO.get());
    err += GET_GL_ERROR();

    // Reserve instance VBO for one instance at a time (no batching; each
    // entity needs unique joint matrices).
    err += m_instanceVBO.Reserve(1);
    OKAMI_ERROR_RETURN(err);

    view.each([&](auto /*entity*/,
                  SkinnedMeshComponent const& mesh,
                  Transform const& transform)
    {
        if (!mesh.m_geometry || !mesh.m_geometry->IsLoaded()) return;

        auto* oglGeo = OGLGeometryManager::GetOGLGeometry(mesh.m_geometry);
        if (!oglGeo || oglGeo->m_meshes.empty()) return;

        // Upload joint matrices; skip entity if not ready.
        if (!UploadJointMatrices(registry, mesh)) return;

        // Write per-entity instance data.
        {
            auto map = m_instanceVBO.Map();
            if (!map) {
                err += map.error();
                return;
            }
            auto matrix       = transform.AsMatrix();
            auto normalMatrix  = glm::transpose(glm::inverse(matrix));
            (*map)[0] = glsl::SkinnedMeshInstance{
                .a_instanceModel_col0   = matrix[0],
                .a_instanceModel_col1   = matrix[1],
                .a_instanceModel_col2   = matrix[2],
                .a_instanceModel_col3   = matrix[3],
                .a_instanceNormal_col0  = normalMatrix[0],
                .a_instanceNormal_col1  = normalMatrix[1],
                .a_instanceNormal_col2  = normalMatrix[2],
                .a_instanceNormal_col3  = normalMatrix[3],
            };
        }
        err += GET_GL_ERROR();

        auto* meshImpl = &oglGeo->m_meshes[0];
        auto* mat = static_cast<OGLMaterial*>(
            mesh.m_material ? mesh.m_material.get() : m_defaultMaterial.get());
        if (!mat) return;

        // Bind VAO + attach per-instance attributes.
        glBindVertexArray(meshImpl->m_vao.get());
        err += GET_GL_ERROR();

        auto const instInfo   = glsl::__get_vs_input_infoSkinnedMeshInstance();
        const GLsizei instStride = static_cast<GLsizei>(instInfo.m_totalStride);

        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO.GetBuffer());
        for (auto const& [location, attrib] : instInfo.locationToAttrib) {
            const GLuint loc = static_cast<GLuint>(location);
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(
                loc,
                static_cast<GLint>(attrib.m_componentCount),
                ToOpenGL(attrib.m_componentType),
                attrib.m_isNormalized ? GL_TRUE : GL_FALSE,
                instStride,
                reinterpret_cast<void*>(static_cast<GLsizeiptr>(attrib.m_offset)));
            glVertexAttribDivisor(loc, 1);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        err += GET_GL_ERROR();

        if (pass.m_type == OGLPassType::Shadow) {
            glUseProgram(m_depthProgram.get());
            err += m_depthPassProvider->GetCascadesBuffer().Bind(
                static_cast<GLint>(DepthBindPoints::Cascades));
        } else {
            // Override the material's program with the skinned one.
            glUseProgram(m_skinnedForwardProgram.get());
            // Still apply material texture bindings (diffuse, normal).
            for (auto& tb : mat->m_textureBindings) {
                if (tb.m_texture == 0 && tb.m_handle && tb.m_handle->IsLoaded()) {
                    tb.m_texture = static_cast<OGLTexture*>(tb.m_handle.get())->m_texture.get();
                }
                glActiveTexture(GL_TEXTURE0 + tb.m_unit);
                glBindTexture(GL_TEXTURE_2D, tb.m_texture);
            }
            for (auto const& setter : mat->m_uniformSetters) setter();
            err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(
                static_cast<GLint>(ForwardBindPoints::SceneGlobals));
        }
        err += GET_GL_ERROR();

        auto const& desc = mesh.m_geometry->GetDesc();
        auto const& prim = desc.m_primitives[0];
        if (prim.m_indices) {
            glDrawElementsInstanced(
                GL_TRIANGLES,
                static_cast<GLsizei>(prim.m_indices->m_count),
                ToOpenGL(prim.m_indices->m_type),
                reinterpret_cast<void*>(prim.m_indices->m_offset),
                1);
        } else {
            glDrawArraysInstanced(GL_TRIANGLES, 0,
                                  static_cast<GLsizei>(prim.m_vertexCount), 1);
        }
        err += GET_GL_ERROR();
    });

    glBindVertexArray(0);
    return err;
}

// ---------------------------------------------------------------------------

std::string OGLSkinnedMeshRenderer::GetName() const {
    return "OGL Skinned Mesh Renderer";
}
