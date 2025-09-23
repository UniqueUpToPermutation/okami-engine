#pragma once

#include "../content.hpp"
#include "../texture.hpp"

#include <bgfx/bgfx.h>

namespace okami {
    struct BgfxTextureImpl {
        bgfx::TextureHandle handle;
        bgfx::UniformHandle sampler;
    };

	class BgfxTextureManager : public ContentModule<Texture, BgfxTextureImpl> {
	protected:
        Expected<std::pair<Texture::Desc, BgfxTextureImpl>> CreateResource(Texture&& data, std::any userData) override;
	};
}