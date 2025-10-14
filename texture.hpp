#pragma once

#include <vector>
#include <span>
#include <filesystem>
#include <any>

#include "common.hpp"

#include <glm/vec2.hpp>

namespace okami {
    enum class TextureType {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_2D_ARRAY,
        TEXTURE_3D,
        TEXTURE_CUBE
    };

    enum class TextureFormat {
        R8,
        RG8,
        RGB8,
        RGBA8,
        R32F,
        RG32F,
        RGB32F,
        RGBA32F,
    };

    struct TextureDesc {
        TextureType type;
        TextureFormat format;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t arraySize;
        uint32_t mipLevels;
    };

    uint32_t GetChannelCount(TextureFormat format);
    uint32_t GetPixelStride(TextureFormat format);
    uint32_t GetTextureSize(TextureDesc const& info);

    struct TextureLoadParams {
        bool m_srgb = false;
    };

    class Texture {
    private:
        TextureDesc m_desc;
        TextureLoadParams m_params;
        std::vector<uint8_t> m_data; // Raw texture data
    public:
        inline Texture(const TextureDesc& info, const TextureLoadParams& params = {}) 
            : m_desc(info), m_params(params), m_data(GetTextureSize(info), 0) {}
        inline Texture(const TextureDesc& info, std::vector<uint8_t>&& data, const TextureLoadParams& params = {}) 
            : m_desc(info), m_params(params), m_data(std::move(data)) {
            // Ensure size is correct
            if (m_data.size() != GetTextureSize(info)) {
                throw std::runtime_error("Texture data size does not match description");
            }
        }

        inline const TextureDesc& GetDesc() const { 
            return m_desc; 
        }

        inline const TextureLoadParams& GetLoadParams() const {
            return m_params;
        }

        size_t GetMipOffset(int mipLevel) const;
        size_t GetMipSize(int mipLevel) const;

        const std::span<uint8_t const> GetData(int mipLevel = 0) const;
        std::span<uint8_t> GetData(int mipLevel = 0);

        static Expected<Texture> FromPNG(const std::filesystem::path& path,
            const TextureLoadParams& params = {});
        static Expected<Texture> FromKTX2(const std::filesystem::path& path,
            const TextureLoadParams& params = {});

        Error SavePNG(const std::filesystem::path& path) const;
        Error SaveKTX2(const std::filesystem::path& path) const;

        using Desc = TextureDesc;
        using LoadParams = TextureLoadParams;
    };
}