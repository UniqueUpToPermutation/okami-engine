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
    size_t GetTextureSize(TextureDesc const& info);
    size_t GetMipSize(TextureDesc const& desc, uint32_t mipLevel);
    size_t GetMipOffset(TextureDesc const& desc, uint32_t mipLevel);
    size_t GetSubresourceIndex(TextureDesc const& desc, uint32_t mipLevel, uint32_t layer);

    struct TextureLoadParams {
        bool m_srgb = false;
    };

    struct SubDesc {
        uint32_t mipLevel;
        uint32_t layer;
        uint32_t offset;
    };

    class Texture {
    private:
        TextureDesc m_desc;
        TextureLoadParams m_params;
        std::vector<uint8_t> m_data; // Raw texture data
        std::vector<SubDesc> m_subDescs;

        void UpdateSubDescs();

    public:
        inline Texture(const TextureDesc& info, const TextureLoadParams& params = {}) 
            : m_desc(info), m_params(params), m_data(GetTextureSize(info), 0) { UpdateSubDescs(); }
        inline Texture(const TextureDesc& info, std::vector<uint8_t>&& data, const TextureLoadParams& params = {}) 
            : m_desc(info), m_params(params), m_data(std::move(data)) {
            // Ensure size is correct
            if (m_data.size() != GetTextureSize(info)) {
                throw std::runtime_error("Texture data size does not match description");
            }
            UpdateSubDescs();
        }

        inline const TextureDesc& GetDesc() const { 
            return m_desc; 
        }

        inline const TextureLoadParams& GetLoadParams() const {
            return m_params;
        }

        const std::span<uint8_t const> GetData(uint32_t mipLevel = 0, uint32_t layer = 0) const;
        std::span<uint8_t> GetData(uint32_t mipLevel = 0, uint32_t layer = 0);

        static Expected<Texture> FromPNG(const std::filesystem::path& path,
            const TextureLoadParams& params = {});
        static Expected<Texture> FromKTX2(const std::filesystem::path& path,
            const TextureLoadParams& params = {});

        Error SavePNG(const std::filesystem::path& path, bool saveMips = false) const;
        Error SaveKTX2(const std::filesystem::path& path) const;

        using Desc = TextureDesc;
        using LoadParams = TextureLoadParams;
    };
}