#pragma once

#include "../module.hpp"
#include "../storage.hpp"
#include "../renderer.hpp"

#include "bgfx_texture_manager.hpp"
#include "bgfx_util.hpp"

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace okami {
    class BgfxSpriteModule : 
        public EngineModule,
        public IRenderModule {
    private:
        AutoHandle<bgfx::ProgramHandle> m_program;
        StorageModule<SpriteComponent>* m_storage = nullptr;
        BgfxTextureManager* m_textureManager = nullptr;
        bgfx::VertexLayout m_vertexLayout;

    protected:
        Error RegisterImpl(ModuleInterface&) override;
        Error StartupImpl(ModuleInterface&) override;
        void ShutdownImpl(ModuleInterface&) override;

        Error ProcessFrameImpl(Time const&, ModuleInterface&) override;
        Error MergeImpl() override;

    public:
        inline BgfxSpriteModule(BgfxTextureManager* textureManager) :
            m_textureManager(textureManager) {
            m_storage = CreateChild<StorageModule<SpriteComponent>>();
        }

        std::string_view GetName() const override;
        Error Pass(Time const& time, ModuleInterface& mi, RenderPassInfo info) override;
    };
}