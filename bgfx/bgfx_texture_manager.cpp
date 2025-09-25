#include "bgfx_texture_manager.hpp"

#include <sstream>
#include <expected>

namespace okami {
    Expected<std::pair<Texture::Desc, BgfxTextureImpl>> BgfxTextureManager::CreateResource(Texture&& data, std::any userData) {
        // Get texture description from the input data
        auto desc = data.GetDesc();
        
        // Validate texture dimensions
        if (desc.width == 0 || desc.height == 0) {
            std::stringstream ss;
            ss << "Invalid texture dimensions: " << desc.width << "x" << desc.height;
            return std::unexpected(Error(ss.str()));
        }
        
        // Determine bgfx texture format
        bgfx::TextureFormat::Enum bgfxFormat;
        switch (desc.format) {
            case TextureFormat::RGBA8:
                bgfxFormat = bgfx::TextureFormat::RGBA8;
                break;
            case TextureFormat::RGB8:
                bgfxFormat = bgfx::TextureFormat::RGB8;
                break;
            case TextureFormat::R8:
                bgfxFormat = bgfx::TextureFormat::R8;
                break;
            default:
                std::stringstream ss;
                ss << "Unsupported texture format: " << static_cast<int>(desc.format);
                return std::unexpected(Error(ss.str()));
        }
        
        // Get texture data
        auto textureData = data.GetData();
        if (textureData.empty()) {
            return std::unexpected(Error("Texture data is empty"));
        }

        auto params = data.GetLoadParams();
        
        // Create bgfx texture flags
        uint64_t flags = BGFX_TEXTURE_NONE;
        if (params.m_srgb) {
            flags |= BGFX_TEXTURE_SRGB;
        }
        
        // Create bgfx memory reference from texture data
        const bgfx::Memory* mem = bgfx::makeRef(textureData.data(), static_cast<uint32_t>(textureData.size()));
        
        bool has_mips = desc.mipLevels > 1;

        // Create the bgfx texture handle
        bgfx::TextureHandle handle = bgfx::createTexture2D(
            static_cast<uint16_t>(desc.width),
            static_cast<uint16_t>(desc.height),
            has_mips,
            1, // numLayers
            bgfxFormat,
            flags,
            mem
        );
        
        // Check if texture creation was successful
        if (!bgfx::isValid(handle)) {
            return std::unexpected(Error("Failed to create bgfx texture"));
        }
        
        // Create the BgfxTextureImpl
        BgfxTextureImpl impl;
        impl.handle = AutoHandle(handle);
        impl.sampler = AutoHandle(bgfx::createUniform("s_tex", bgfx::UniformType::Sampler));

        // Return the descriptor and implementation
        return std::make_pair(std::move(desc), std::move(impl));
    }
}