#include "ogl_sky.hpp"

using namespace okami;

#include "ogl_static_mesh.hpp"

#include <glog/logging.h>

using namespace okami;

Error OGLSkyDefaultMaterialManager::RegisterImpl(InterfaceCollection& interfaces) {
    MaterialModuleBase::RegisterImpl(interfaces);
    interfaces.Register<IOGLMaterialManager>(this);
    return {};
}

EmptyMaterialImpl OGLSkyDefaultMaterialManager::CreateImpl(SkyDefaultMaterial const& material) {
    return EmptyMaterialImpl{};
}
void OGLSkyDefaultMaterialManager::DestroyImpl(EmptyMaterialImpl& impl) {
}
ProgramShaderPaths OGLSkyDefaultMaterialManager::GetShaderPaths() const {
    return ProgramShaderPaths{
        .m_fragment = GetGLSLShaderPath("sky_default.fs")
    };
}
Error OGLSkyDefaultMaterialManager::Bind(MaterialHandle handle, OGLMaterialBindParams const& params) const {
    return {};
}
Error OGLSkyDefaultMaterialManager::OnProgramCreated(GLProgram const& program, 
    OGLMaterialBindParams const& params) const {
    return {};
}


Error OGLSkyRenderer::StartupImpl(InitContext const& context) {
    Error err;
    
    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLSpriteRenderer");

    auto vertexShaderPath = GetGLSLShaderPath("sky.vs");
    context.m_interfaces.ForEachInterface<IOGLMaterialManager>([&](IOGLMaterialManager* manager) {
        if (manager->GetVertexShaderOutput() != OGLVertexShaderOutput::Sky) {
            return;
        }

        auto shaders = manager->GetShaderPaths();
        if (shaders.m_vertex.empty()) {
            shaders.m_vertex = vertexShaderPath;
        }

        // Create shader program with vertex, geometry, and fragment shaders
        auto program = CreateProgram(shaders, *cache);
        if (!program) {
            LOG(ERROR) << "Sky Renderer: Failed to create shader program for material: " << manager->GetMaterialName() << 
                " with error: " << program.error();
            err += program.error();
            return;
        }

        // Assign binding points
        glUseProgram((*program).get());
        err += GET_GL_ERROR();
        err += AssignBufferBindingPoint(*program, "SceneGlobalsBlock",  BufferBindingPoints::SceneGlobals);
        err += manager->OnProgramCreated(*program, GetMaterialBindParams());

        m_programs.emplace(manager->GetMaterialType(), 
            MaterialImpl{
                .m_program = std::move(*program), 
                .m_manager = manager
            });
    });

    OKAMI_ERROR_RETURN(err);

    auto sceneUBO = UniformBuffer<glsl::SceneGlobals>::Create();
    OKAMI_ERROR_RETURN(sceneUBO);
    m_sceneUBO = std::move(*sceneUBO);

    m_pipelineState.depthTestEnabled = true;
    m_pipelineState.blendEnabled = false;
    m_pipelineState.cullFaceEnabled = true;
    m_pipelineState.depthMask = true;
    m_pipelineState.depthFunc = GL_LEQUAL;

    LOG(INFO) << "OGL Sky Renderer initialized successfully";
    return err;
}

OGLSkyRenderer::OGLSkyRenderer() {
    m_storage = CreateChild<StorageModule<SkyComponent>>();
}

Error OGLSkyRenderer::Pass(OGLPass const& pass) {
    Error err;

    // Set up rendering state
    m_pipelineState.SetToGL(); 
    err += GET_GL_ERROR();
    err += m_sceneUBO.Write(pass.m_sceneGlobals);

    okami::PrimitiveImpl* currentMeshImpl = nullptr;

    ResHandle<Geometry> currentGeometry;
    std::optional<MaterialHandle> currentMaterial;

    m_storage->ForEach([&](auto const& entity, SkyComponent const& component) {
        auto currentMaterial = component.m_skyMaterial;

        // If no material, render with default material.
        auto it = m_programs.find(currentMaterial.Ptr() ? currentMaterial.Ptr()->m_materialType : m_defaultMaterialType);
        if (it != m_programs.end()) {
            glUseProgram(it->second.m_program.get());
            err += GET_GL_ERROR();

            err += m_sceneUBO.Bind(BufferBindingPoints::SceneGlobals);
            err += it->second.m_manager->Bind(currentMaterial, GetMaterialBindParams());

            currentMaterial = currentMaterial;
        } else {
            OKAMI_LOG_WARNING("No shader program found for material type");
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    });
    
    return err;
}

std::string OGLSkyRenderer::GetName() const {
    return "OGL Sky Renderer";
}