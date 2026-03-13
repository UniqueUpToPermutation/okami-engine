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
    for (auto const& setter : m_uniformSetters) {
        setter();
    }
    for (auto& tb : m_textureBindings) {
        // Lazily resolve the GL handle once the texture finishes loading.
        if (tb.m_texture == 0 && tb.m_handle && tb.m_handle->IsLoaded()) {
            tb.m_texture = static_cast<OGLTexture*>(tb.m_handle.get())->m_texture.get();
        }
        glActiveTexture(GL_TEXTURE0 + tb.m_unit);
        glBindTexture(GL_TEXTURE_2D, tb.m_texture);
    }
}

// ---------------------------------------------------------------------------
// OGLMaterialManager
// ---------------------------------------------------------------------------

OGLMaterialManager::OGLMaterialManager()
{}

Error OGLMaterialManager::RegisterImpl(InterfaceCollection& interfaces) {
    interfaces.Register<IMaterialManager<DefaultMaterial>>(this);
    interfaces.Register<IMaterialManager<LambertMaterial>>(this);
    interfaces.Register<IMaterialManager<SkyDefaultMaterial>>(this);
    interfaces.Register<IMaterialManager<SkyAtmosphereMaterial>>(this);
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
        return AssignBufferBindingPoint(prog, "SceneGlobalsBlock", 0);
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
        // LambertMaterial – Lambertian + optional normal map
        {
            typeid(LambertMaterial),
            OGLRendererType::StaticMesh,
            GetGLSLShaderPath("static_mesh.vs"),
            GetGLSLShaderPath("lambert.fs"),
            [setupStaticMesh](GLProgram const& prog) -> Error {
                auto e = setupStaticMesh(prog);
                e += AssignBufferBindingPoint(prog, "ShadowCameraBlock", 1);
                e += AssignTextureBindingPoint(prog, "u_diffuseMap", 0);
                e += AssignTextureBindingPoint(prog, "u_normalMap",  1);
                e += AssignTextureBindingPoint(prog, "u_shadowMap",  2);
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
        // SkyAtmosphereMaterial – Preetham / Mie scattering atmosphere
        {
            typeid(SkyAtmosphereMaterial),
            OGLRendererType::Sky,
            GetGLSLShaderPath("sky.vs"),
            GetGLSLShaderPath("sky_atmosphere.fs"),
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

    // Create a 1x1 flat-normal texture used as the unit-1 fallback for
    // materials that don't have a normal map.  Decodes to tangent-space (0,0,1)
    // which leaves the geometric normal unchanged.
    {
        GLuint id;
        glGenTextures(1, &id);
        m_flatNormalTexture = GLTexture(id);
        glBindTexture(GL_TEXTURE_2D, id);
        const uint8_t pixel[4] = { 128, 128, 255, 255 }; // (0.5, 0.5, 1.0, 1.0)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

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

MaterialHandle OGLMaterialManager::CreateMaterial(LambertMaterial material) {
    auto* entry = GetProgramEntry(typeid(LambertMaterial));
    auto mat = std::make_shared<OGLMaterial>(
        OGLRendererType::StaticMesh,
        typeid(LambertMaterial),
        entry ? &entry->m_program : nullptr);

    // Unit 0: diffuse/albedo
    if (material.m_colorTexture) {
        GLuint glId = 0;
        if (material.m_colorTexture->IsLoaded())
            glId = static_cast<OGLTexture*>(material.m_colorTexture.get())->m_texture.get();
        mat->m_textureBindings.push_back({ 0, glId, material.m_colorTexture });
    }

    // Unit 1: normal map (fall back to flat-normal 1x1 if absent)
    if (material.m_normalTexture) {
        GLuint glId = 0;
        if (material.m_normalTexture->IsLoaded())
            glId = static_cast<OGLTexture*>(material.m_normalTexture.get())->m_texture.get();
        mat->m_textureBindings.push_back({ 1, glId, material.m_normalTexture });
    } else {
        mat->m_textureBindings.push_back({ 1, m_flatNormalTexture.get(), nullptr });
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

MaterialHandle OGLMaterialManager::CreateMaterial(SkyAtmosphereMaterial material) {
    auto* entry = GetProgramEntry(typeid(SkyAtmosphereMaterial));
    auto mat = std::make_shared<OGLMaterial>(
        OGLRendererType::Sky,
        typeid(SkyAtmosphereMaterial),
        entry ? &entry->m_program : nullptr);

    if (entry) {
        GLuint prog = entry->m_program.get();

        auto add1f = [&](const char* name, float value) {
            GLint loc = glGetUniformLocation(prog, name);
            if (loc != -1)
                mat->m_uniformSetters.push_back([loc, value]() { glUniform1f(loc, value); });
        };
        auto add3f = [&](const char* name, glm::vec3 value) {
            GLint loc = glGetUniformLocation(prog, name);
            if (loc != -1)
                mat->m_uniformSetters.push_back([loc, value]() { glUniform3f(loc, value.x, value.y, value.z); });
        };

        add1f("depolarizationFactor",        material.depolarizationFactor);
        add1f("mieCoefficient",              material.mieCoefficient);
        add1f("mieDirectionalG",             material.mieDirectionalG);
        add3f("mieKCoefficient",             material.mieKCoefficient);
        add1f("mieV",                        material.mieV);
        add1f("mieZenithLength",             material.mieZenithLength);
        add1f("numMolecules",                material.numMolecules);
        add3f("primaries",                   material.primaries);
        add1f("rayleigh",                    material.rayleigh);
        add1f("rayleighZenithLength",        material.rayleighZenithLength);
        add1f("refractiveIndex",             material.refractiveIndex);
        add1f("sunAngularDiameterDegrees",   material.sunAngularDiameterDegrees);
        add1f("sunIntensityFactor",          material.sunIntensityFactor);
        add1f("sunIntensityFalloffSteepness",material.sunIntensityFalloffSteepness);
        add3f("sunPosition",                 material.sunPosition);
        add1f("turbidity",                   material.turbidity);
    }

    return mat;
}

MaterialHandle OGLMaterialManager::CreateDefaultMaterial(OGLRendererType renderer) {
    switch (renderer) {
        case OGLRendererType::StaticMesh: return CreateMaterial(DefaultMaterial{});
        case OGLRendererType::Sky:        return CreateMaterial(SkyDefaultMaterial{});
    }
    return nullptr;
}
