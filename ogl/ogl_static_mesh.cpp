#include "ogl_static_mesh.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLStaticMeshRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    m_geometryManager->SetStaticMeshVSInput(glsl::__get_vs_input_infoStaticMeshVertex());

    return {};
}

Error OGLStaticMeshRenderer::StartupImpl(InitContext const& context) {
    Error err;
    
    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLSpriteRenderer");

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider, "IOGLSceneGlobalsProvider interface not available for OGLStaticMeshRenderer");

    auto vertexShaderPath = GetGLSLShaderPath("static_mesh.vs");
    context.m_interfaces.ForEachInterface<IOGLMaterialManager>([&](IOGLMaterialManager* manager) {
        if (manager->GetVertexShaderOutput() != OGLVertexShaderOutput::Mesh) {
            return;
        }
        
        auto shaders = manager->GetShaderPaths();
        if (shaders.m_vertex.empty()) {
            shaders.m_vertex = vertexShaderPath;
        }

        // Create shader program with vertex, geometry, and fragment shaders
        auto program = CreateProgram(shaders, *cache);
        if (!program) {
            LOG(ERROR) << "Static Mesh Renderer: Failed to create shader program for material: " << manager->GetMaterialName() << 
                " with error: " << program.error();
            err += program.error();
            return;
        }

        // Assign binding points
        glUseProgram((*program).get());
        err += GET_GL_ERROR();
        err += AssignBufferBindingPoint(*program, "SceneGlobalsBlock",  BufferBindingPoints::SceneGlobals);
        err += AssignBufferBindingPoint(*program, "StaticMeshInstanceBlock", BufferBindingPoints::StaticMeshInstance);
        err += manager->OnProgramCreated(*program, GetMaterialBindParams());

        m_programs.emplace(manager->GetMaterialType(), 
            MaterialImpl{
                .m_program = std::move(*program), 
                .m_manager = manager
            });
    });

    OKAMI_ERROR_RETURN(err);

    auto instanceUBO = UniformBuffer<glsl::StaticMeshInstance>::Create();
    OKAMI_ERROR_RETURN(instanceUBO);
    m_instanceUBO = std::move(*instanceUBO);

    m_pipelineState.depthTestEnabled = true;
    m_pipelineState.blendEnabled = false;
    m_pipelineState.cullFaceEnabled = true;
    m_pipelineState.depthMask = true;

    LOG(INFO) << "OGL Static Mesh Renderer initialized successfully";
    return err;
}

OGLStaticMeshRenderer::OGLStaticMeshRenderer(OGLGeometryManager* geometryManager) :
    m_geometryManager(geometryManager) {
}

Error OGLStaticMeshRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    struct InstanceData {
        ResHandle<Geometry> m_geometry;
        MaterialHandle m_material;
        glsl::StaticMeshInstance m_glslData;
    };

    // Collect all sprites and their transforms
    std::vector<InstanceData> instances;

    registry.view<StaticMeshComponent, Transform>().each(
        [&](auto entity, StaticMeshComponent const& mesh, Transform const& transform) {
            // Skip sprites without textures
            if (!mesh.m_geometry || !mesh.m_geometry.IsLoaded()) {
                return;
            }
            
            auto matrix = transform.AsMatrix();
            instances.emplace_back(InstanceData{
                .m_geometry = mesh.m_geometry,
                .m_material = mesh.m_material,
                .m_glslData = glsl::StaticMeshInstance{
                    .u_model = matrix,
                    .u_normalMatrix = glm::transpose(glm::inverse(matrix)),
                }
            });
        });

    if (instances.empty()) {
        return {};
    }

    Error err;

    // Sort instances by geometry and material to minimize state changes
    std::sort(instances.begin(), instances.end(),
        [this](const InstanceData& a, const InstanceData& b) {
            if (a.m_geometry == b.m_geometry) {
                return a.m_material.GetEntity() < b.m_material.GetEntity();
            } else {
                return a.m_geometry.GetEntity() < b.m_geometry.GetEntity();
            }
        });

    // Set up rendering state
    m_pipelineState.SetToGL(); 
    err += GET_GL_ERROR();
    err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);

    okami::PrimitiveImpl* currentMeshImpl = nullptr;

    ResHandle<Geometry> currentGeometry;
    std::optional<MaterialHandle> currentMaterial;

    for (auto const& inst : instances) {
        // If there's no geometry, skip
        if (!inst.m_geometry) {
            continue;
        }
        
        // Check if state change is needed
        if (inst.m_geometry != currentGeometry) {
            currentMeshImpl = &m_geometryManager->GetImpl(inst.m_geometry)->m_meshes[0];
            glBindVertexArray(currentMeshImpl->m_vao.get());
            err += GET_GL_ERROR();
        
            currentGeometry = inst.m_geometry;
        }

        // Check if state change is needed
        if (!currentMaterial || inst.m_material != *currentMaterial) {
            // If no material, render with default material.
            auto it = m_programs.find(inst.m_material.Ptr() ? inst.m_material.Ptr()->m_materialType : m_defaultMaterialType);
            if (it != m_programs.end()) {
                glUseProgram(it->second.m_program.get());
                err += GET_GL_ERROR();

                err += m_instanceUBO.Bind(BufferBindingPoints::StaticMeshInstance);
                err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);
                err += it->second.m_manager->Bind(inst.m_material, GetMaterialBindParams());

                currentMaterial = inst.m_material;
            } else {
                OKAMI_LOG_WARNING("No shader program found for material type");
            }
        }

        // Write GLSL instance data and render the mesh
        if (currentMeshImpl) {
            err += m_instanceUBO.Write(inst.m_glslData);
            auto const& desc = currentGeometry.GetDesc();

            auto const& primitive = desc.m_primitives[0];
            
            if (primitive.m_indices) {
                GLint indexType = ToOpenGL(primitive.m_indices->m_type);

                glDrawElements(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(primitive.m_indices->m_count),
                    indexType,
                    (void*)primitive.m_indices->m_offset);
                err += GET_GL_ERROR();
            } else {
                glDrawArrays(
                    GL_TRIANGLES,
                    0,
                    static_cast<GLsizei>(primitive.m_vertexCount)
                );
                err += GET_GL_ERROR();
            }
        }
    }
    
    return err;
}

std::string OGLStaticMeshRenderer::GetName() const {
    return "OGL Static Mesh Renderer";
}