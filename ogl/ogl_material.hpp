#pragma once

#include "ogl_utils.hpp"
#include "ogl_texture.hpp"

#include "../material.hpp"
#include "../sky.hpp"

#include <unordered_map>
#include <vector>

namespace okami {
    // Identifies which renderer a material is paired with.  This determines
    // which vertex shader is used when compiling the GL program at startup.
    enum class OGLRendererType {
        StaticMesh,
        Sky,
    };

    // Binds a single GL texture to a specific texture unit.
    // Holds a TextureHandle to keep the texture alive for as long as the
    // material exists.
    struct OGLTextureBinding {
        GLint         m_unit    = 0;
        GLuint        m_texture = 0;
        TextureHandle m_handle;           // owns the ref-count
    };

    // The single concrete OpenGL material implementation.
    //
    // Each instance holds a (non-owning) pointer to the shared GL program
    // compiled for its material type plus the per-instance texture bindings.
    // Calling Bind() activates the program and sets up all texture units.
    class OGLMaterial : public IMaterial {
    public:
        OGLRendererType              m_rendererType;
        std::type_index              m_type;
        GLProgram const*             m_program = nullptr; // non-owning – owned by OGLMaterialManager
        mutable std::vector<OGLTextureBinding> m_textureBindings;

        OGLMaterial(OGLRendererType rendererType,
                    std::type_index  type,
                    GLProgram const* program)
            : m_rendererType(rendererType)
            , m_type(type)
            , m_program(program) {}

        std::type_index GetType() const override { return m_type; }

        // Activates the GL program and binds all texture units.
        void Bind() const;
    };

    // The single OpenGL material manager.
    //
    // At startup it compiles one GL program per material type (pairing each
    // type's fragment shader with the vertex shader of its target renderer).
    // CreateMaterial() converts a frontend material struct into a live
    // OGLMaterial whose lifetime is managed by the returned MaterialHandle.
    class OGLMaterialManager final :
        public EngineModule,
        public IMaterialManager<DefaultMaterial>,
        public IMaterialManager<LambertMaterial>,
        public IMaterialManager<SkyDefaultMaterial> {
    private:
        struct ProgramEntry {
            GLProgram       m_program;
            OGLRendererType m_renderer;
        };

        std::unordered_map<std::type_index, ProgramEntry> m_programs;
        GLTexture m_flatNormalTexture; // 1x1 (0.5,0.5,1.0) fallback for materials without a normal map

        ProgramEntry const* GetProgramEntry(std::type_index type) const;

    public:
        explicit OGLMaterialManager();

        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

        MaterialHandle CreateMaterial(DefaultMaterial        material) override;
        MaterialHandle CreateMaterial(LambertMaterial  material) override;
        MaterialHandle CreateMaterial(SkyDefaultMaterial     material) override;

        // Creates the default material for the given renderer type.
        MaterialHandle CreateDefaultMaterial(OGLRendererType renderer);
    };
}