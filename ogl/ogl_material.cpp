#include "ogl_material.hpp"
#include "../paths.hpp"

#include <glog/logging.h>

using namespace okami;

// ---------------------------------------------------------------------------
// OGLMaterial
// ---------------------------------------------------------------------------

void OGLMaterial::Bind() const {
    if (m_program) {
        glUseProgram(m_program->get());
    }
    for (auto const& tb : m_textureBindings) {
        glActiveTexture(GL_TEXTURE0 + tb.m_unit);
        glBindTexture(GL_TEXTURE_2D, tb.m_texture);
    }
}

// ---------------------------------------------------------------------------
// OGLMaterialManager
// ---------------------------------------------------------------------------

OGLMaterialManager::OGLMaterialManager(OGLTextureManager* textureManager)
    : m_textureManager(textureManager) {}

Error OGLMaterialManager::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IMaterialManager<DefaultMaterial>>(this);
    interfaces.Register<IMaterialManager<BasicTexturedMaterial>>(this);
    interfaces.Register<IMaterialManager<SkyDefaultMaterial>>(this);
    return {};
}

Error OGLMaterialManager::StartupImpl(InitContext const& context) {
    Error err;

    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "OGLMaterialManager: IGLShaderCache interface not available");

    struct MaterialTypeDesc {
        std::type_index                        m_type;
        OGLRendererType                        m_renderer;
        std::filesystem::path                  m_vertexShader;
        std::optional<std::filesystem::path>   m_fragmentShader;
        std::function<Error(GLProgram const&)> m_onCreated;
    };

    auto setupStaticMesh = [](GLProgram const& prog) -> Error {
        Error e;
        e += AssignBufferBindingPoint(prog, "SceneGlobalsBlock",       0);
        e += AssignBufferBindingPoint(prog, "StaticMeshInstanceBlock", 1);
        return e;
    };
    auto setupSky = [](GLProgram const& prog) -> Error {
        return AssignBufferBindingPoint(prog, "SceneGlobalsBlock", 0);
    };

    std::vector<MaterialTypeDesc> descs = {
        // DefaultMaterial – static mesh, no textures
        {
            typeid(DefaultMaterial),
            OGLRendererType::StaticMesh,
            GetGLSLShaderPath("static_mesh.vs"),
            GetGLSLShaderPath("material_default.fs"),
            setupStaticMesh,
        },
        // BasicTexturedMaterial – static mesh, one diffuse texture at unit 0
        {
            typeid(BasicTexturedMaterial),
            OGLRendererType::StaticMesh,
            GetGLSLShaderPath("static_mesh.vs"),
            GetGLSLShaderPath("material_basic_textured.fs"),
            [setupStaticMesh](GLProgram const& prog) -> Error {
                auto e = setupStaticMesh(prog);
                e += AssignTextureBindingPoint(prog, "u_diffuseMap", 0);
                return e;
            },
        },
        // SkyDefaultMaterial – sky renderer, no textures
        {
            typeid(SkyDefaultMaterial),
            OGLRendererType::Sky,
            GetGLSLShaderPath("sky.vs"),
            GetGLSLShaderPath("sky_default.fs"),
            setupSky,
        },
    };

    for (auto const& desc : descs) {
        ProgramShaderPaths paths;
        paths.m_vertex   = desc.m_vertexShader;
        paths.m_fragment = desc.m_fragmentShader;

        auto program = CreateProgram(paths, *cache);
        if (!program) {
            LOG(ERROR) << "OGLMaterialManager: Failed to compile program for "
                       << desc.m_type.name() << ": " << program.error();
            err += program.error();
            continue;
        }

        glUseProgram((*program).get());
        err += GET_GL_ERROR();

        if (desc.m_onCreated) {
            err += desc.m_onCreated(*program);
        }

        m_programs.emplace(desc.m_type,
            ProgramEntry{ .m_program = std::move(*program), .m_renderer = desc.m_renderer });
    }

    glUseProgram(0);
    return err;
}

OGLMaterialManager::ProgramEntry const*
OGLMaterialManager::GetProgramEntry(std::type_index type) const {
    auto it = m_programs.find(type);
    return it != m_programs.end() ? &it->second : nullptr;
}

MaterialHandle OGLMaterialManager::CreateMaterial(DefaultMaterial /*material*/) {
    auto* entry = GetProgramEntry(typeid(DefaultMaterial));
    return std::make_shared<OGLMaterial>(
        OGLRendererType::StaticMesh,
        typeid(DefaultMaterial),
        entry ? &entry->m_program : nullptr);
}

MaterialHandle OGLMaterialManager::CreateMaterial(BasicTexturedMaterial material) {
    auto* entry = GetProgramEntry(typeid(BasicTexturedMaterial));
    auto mat = std::make_shared<OGLMaterial>(
        OGLRendererType::StaticMesh,
        typeid(BasicTexturedMaterial),
        entry ? &entry->m_program : nullptr);

    if (material.m_colorTexture && material.m_colorTexture.IsLoaded()) {
        if (auto* texImpl = m_textureManager->GetImpl(material.m_colorTexture)) {
            mat->m_textureBindings.push_back({ 0, texImpl->m_texture.get() });
        }
    }
    return mat;
}

MaterialHandle OGLMaterialManager::CreateMaterial(SkyDefaultMaterial /*material*/) {
    auto* entry = GetProgramEntry(typeid(SkyDefaultMaterial));
    return std::make_shared<OGLMaterial>(
        OGLRendererType::Sky,
        typeid(SkyDefaultMaterial),
        entry ? &entry->m_program : nullptr);
}

MaterialHandle OGLMaterialManager::CreateDefaultMaterial(OGLRendererType renderer) {
    switch (renderer) {
        case OGLRendererType::StaticMesh: return CreateMaterial(DefaultMaterial{});
        case OGLRendererType::Sky:        return CreateMaterial(SkyDefaultMaterial{});
    }
    return nullptr;
}
