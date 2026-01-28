#pragma once

#include "ogl_utils.hpp"

#include "../module.hpp"

namespace okami {
    class IOGLCookTorrenceBRDFProvider {
    public:
        virtual GLTexture const& GetBrdfLut() const = 0;

        virtual ~IOGLCookTorrenceBRDFProvider() = default;
    };

    class OGLBrdfProvider final : public EngineModule,
        public IOGLCookTorrenceBRDFProvider {
    private:
        GLTexture m_brdfLut;
        bool m_debugMode = false;

    public:
        Error RegisterImpl(InterfaceCollection& interfaces) override;
        Error StartupImpl(InitContext const& context) override;

        GLTexture const& GetBrdfLut() const override {
            return m_brdfLut;
        }

        inline OGLBrdfProvider(bool debugMode = false) : m_debugMode(debugMode) {}
    };

    constexpr int kBrdfLutSize = 256;
    Expected<GLTexture> CreateBrdfLut(IGLShaderCache& cache, bool bDebugMode = false);
}