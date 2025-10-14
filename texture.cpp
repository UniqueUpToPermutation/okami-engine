#include <cmath>
#include <algorithm>
#include <fstream>

#include <glog/logging.h>

#define USE_KTX

#ifdef USE_KTX
#include <ktx.h>
#endif

#include "texture.hpp"
#include "lodepng.h"

#ifdef USE_KTX
// Vulkan format constants (from vulkan_core.h)
// These values are part of the Vulkan specification and are stable
namespace VkFormat {
    constexpr ktx_uint32_t VK_FORMAT_UNDEFINED = 0;
    constexpr ktx_uint32_t VK_FORMAT_R8_UNORM = 9;
    constexpr ktx_uint32_t VK_FORMAT_R8G8_UNORM = 16;
    constexpr ktx_uint32_t VK_FORMAT_R8G8B8_UNORM = 23;
    constexpr ktx_uint32_t VK_FORMAT_R8G8B8_SRGB = 29;
    constexpr ktx_uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
    constexpr ktx_uint32_t VK_FORMAT_R32_SFLOAT = 100;
    constexpr ktx_uint32_t VK_FORMAT_R32G32_SFLOAT = 103;
    constexpr ktx_uint32_t VK_FORMAT_R32G32B32_SFLOAT = 106;
    constexpr ktx_uint32_t VK_FORMAT_R32G32B32A32_SFLOAT = 109;
}
#endif

namespace okami {

#ifdef USE_KTX
// Helper function to convert TextureFormat to Vulkan format
ktx_uint32_t TextureFormatToVkFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
            return VkFormat::VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8:
            return VkFormat::VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8:
            return VkFormat::VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::RGBA8:
            return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::R32F:
            return VkFormat::VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32F:
            return VkFormat::VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32F:
            return VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32F:
            return VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            return VkFormat::VK_FORMAT_UNDEFINED;
    }
}

// Helper function to convert Vulkan format to TextureFormat
TextureFormat VkFormatToTextureFormat(ktx_uint32_t vkFormat) {
    switch (vkFormat) {
        case VkFormat::VK_FORMAT_R8_UNORM:
            return TextureFormat::R8;
        case VkFormat::VK_FORMAT_R8G8_UNORM:
            return TextureFormat::RG8;
        case VkFormat::VK_FORMAT_R8G8B8_UNORM:
        case VkFormat::VK_FORMAT_R8G8B8_SRGB:
            return TextureFormat::RGB8;
        case VkFormat::VK_FORMAT_R8G8B8A8_UNORM:
            return TextureFormat::RGBA8;
        case VkFormat::VK_FORMAT_R32_SFLOAT:
            return TextureFormat::R32F;
        case VkFormat::VK_FORMAT_R32G32_SFLOAT:
            return TextureFormat::RG32F;
        case VkFormat::VK_FORMAT_R32G32B32_SFLOAT:
            return TextureFormat::RGB32F;
        case VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT:
            return TextureFormat::RGBA32F;
        default:
            return TextureFormat::RGBA8; // Default fallback
    }
}
#endif

uint32_t GetChannelCount(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::R32F:
            return 1;
        case TextureFormat::RG8:
        case TextureFormat::RG32F:
            return 2;
        case TextureFormat::RGB8:
        case TextureFormat::RGB32F:
            return 3;
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA32F:
            return 4;
        default:
            return 0;
    }
}

uint32_t GetPixelStride(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
            return 1;
        case TextureFormat::RG8:
            return 2;
        case TextureFormat::RGB8:
            return 3;
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::R32F:
            return 4;
        case TextureFormat::RG32F:
            return 8;
        case TextureFormat::RGB32F:
            return 12;
        case TextureFormat::RGBA32F:
            return 16;
        default:
            return 0;
    }
}

uint32_t GetTextureSize(const TextureDesc& info) {
    uint32_t pixelStride = GetPixelStride(info.format);
    uint32_t totalSize = 0;
    
    // Calculate size for all mip levels
    for (uint32_t mip = 0; mip < info.mipLevels; ++mip) {
        uint32_t mipWidth = std::max(1u, info.width >> mip);
        uint32_t mipHeight = std::max(1u, info.height >> mip);
        uint32_t mipDepth = std::max(1u, info.depth >> mip);
        
        uint32_t mipSize = mipWidth * mipHeight * mipDepth * pixelStride;
        
        // For texture arrays, multiply by array size
        if (info.type == TextureType::TEXTURE_2D_ARRAY) {
            mipSize *= info.arraySize;
        }
        
        totalSize += mipSize;
    }
    
    return totalSize;
}

Expected<Texture> Texture::FromPNG(const std::filesystem::path& path,
    const TextureLoadParams& params) {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        return std::unexpected(Error("PNG file does not exist: " + path.string()));
    }
    
    // Convert path to string for lodepng (which expects char*)
    std::string pathStr = path.string();
    
    unsigned char* imageData = nullptr;
    unsigned width, height;
    
    // Load PNG with 32-bit RGBA format
    unsigned error = lodepng_decode32_file(&imageData, &width, &height, pathStr.c_str());
    
    if (error) {
        return std::unexpected(Error("Failed to load PNG: " + std::string(lodepng_error_text(error))));
    }
    
    // Ensure we have valid data
    if (!imageData || width == 0 || height == 0) {
        if (imageData) {
            free(imageData);
        }
        return std::unexpected(Error("Invalid PNG data"));
    }
    
    // Create texture info - PNG loaded as RGBA8, 2D texture, single mip level
    TextureDesc info = {};
    info.type = TextureType::TEXTURE_2D;
    info.format = TextureFormat::RGBA8;
    info.width = width;
    info.height = height;
    info.depth = 1;
    info.arraySize = 1;
    info.mipLevels = 1;
    
    // Create texture
    Texture texture(info, params);
    
    // Copy data to texture
    uint32_t dataSize = width * height * 4; // RGBA8 = 4 bytes per pixel
    std::copy(imageData, imageData + dataSize, texture.m_data.begin());
    
    // Free lodepng allocated memory
    free(imageData);
    
    return texture;
}

#ifdef USE_KTX
Expected<Texture> Texture::FromKTX2(const std::filesystem::path& path,
    const TextureLoadParams& params) {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        return std::unexpected(Error("KTX2 file does not exist: " + path.string()));
    }
    
    // Load KTX texture from file
    ktxTexture* ktxTex = nullptr;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        path.string().c_str(), 
        KTX_TEXTURE_CREATE_NO_FLAGS, 
        &ktxTex
    );
    
    if (result != KTX_SUCCESS) {
        return std::unexpected(Error("Failed to load KTX2 file: " + path.string() + " (error code: " + std::to_string(result) + ")"));
    }
    
    if (!ktxTex) {
        return std::unexpected(Error("KTX texture is null"));
    }
    
    // Determine texture type
    TextureType type = TextureType::TEXTURE_2D;
    if (ktxTex->isCubemap) {
        type = TextureType::TEXTURE_CUBE;
    } else if (ktxTex->isArray) {
        type = TextureType::TEXTURE_2D_ARRAY;
    } else if (ktxTex->baseDepth > 1) {
        type = TextureType::TEXTURE_3D;
    } else if (ktxTex->baseHeight == 1 && ktxTex->baseDepth == 1) {
        type = TextureType::TEXTURE_1D;
    }
    
    // Determine texture format based on KTX version
    TextureFormat format;
    if (ktxTex->classId == ktxTexture2_c) {
        ktxTexture2* ktx2 = (ktxTexture2*)ktxTex;
        format = VkFormatToTextureFormat(ktx2->vkFormat);
    } else {
        // For KTX1, we'll default to RGBA8 as it's most common
        format = TextureFormat::RGBA8;
        LOG(WARNING) << "Loading KTX1 file, defaulting to RGBA8 format";
    }
    
    // Create texture description
    TextureDesc info = {};
    info.type = type;
    info.format = format;
    info.width = ktxTex->baseWidth;
    info.height = ktxTex->baseHeight;
    info.depth = ktxTex->baseDepth;
    info.arraySize = ktxTex->numLayers;
    info.mipLevels = ktxTex->numLevels;
    
    // Create texture object
    Texture texture(info, params);
    
    // Load the image data first if needed
    if (!ktxTex->pData) {
        result = ktxTexture_LoadImageData((ktxTexture*)ktxTex, nullptr, 0);
        if (result != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTex);
            return std::unexpected(Error("Failed to load KTX image data (error code: " + std::to_string(result) + ")"));
        }
    }

    // Load the image data into our texture
    ktx_size_t dataSize = ktxTex->dataSize;
    if (ktxTex->pData && dataSize > 0) {
        // Copy data to our texture
        if (dataSize > texture.m_data.size()) {
            LOG(WARNING) << "KTX data size (" << dataSize << ") larger than expected texture size (" << texture.m_data.size() << "), truncating";
            dataSize = texture.m_data.size();
        }
        
        std::copy(ktxTex->pData, ktxTex->pData + dataSize, texture.m_data.begin());
    } else {
        ktxTexture_Destroy(ktxTex);
        return std::unexpected(Error("KTX texture has no data after loading"));
    }
    
    // Clean up KTX texture
    ktxTexture_Destroy(ktxTex);
    
    return texture;
}
#else
Expected<Texture> Texture::FromKTX2(const std::filesystem::path& path,
    const TextureLoadParams& params) {
    return std::unexpected(Error("KTX2 support not compiled in"));
}
#endif

Error Texture::SavePNG(const std::filesystem::path& path) const {
    // PNG only supports certain formats, so we need to convert
    std::vector<uint8_t> pngData;
    unsigned width = m_desc.width;
    unsigned height = m_desc.height;
    LodePNGColorType colorType;
    unsigned bitDepth = 8;

    // Convert texture data to PNG-compatible format
    switch (m_desc.format) {
        case TextureFormat::R8: {
            colorType = LCT_GREY;
            pngData = m_data; // Direct copy for single channel 8-bit
            break;
        }
        case TextureFormat::RG8: {
            colorType = LCT_GREY_ALPHA;
            pngData = m_data; // Direct copy for two channel 8-bit
            break;
        }
        case TextureFormat::RGB8: {
            colorType = LCT_RGB;
            pngData = m_data; // Direct copy for three channel 8-bit
            break;
        }
        case TextureFormat::RGBA8: {
            colorType = LCT_RGBA;
            pngData = m_data; // Direct copy for four channel 8-bit
            break;
        }
        case TextureFormat::R32F: {
            // Convert R32F to R8
            colorType = LCT_GREY;
            pngData.resize(width * height);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        case TextureFormat::RG32F: {
            // Convert RG32F to RG8
            colorType = LCT_GREY_ALPHA;
            pngData.resize(width * height * 2);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height * 2; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        case TextureFormat::RGB32F: {
            // Convert RGB32F to RGB8
            colorType = LCT_RGB;
            pngData.resize(width * height * 3);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height * 3; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        case TextureFormat::RGBA32F: {
            // Convert RGBA32F to RGBA8
            colorType = LCT_RGBA;
            pngData.resize(width * height * 4);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height * 4; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        default:
            return Error("Unsupported texture format for PNG export: " + std::to_string(static_cast<int>(m_desc.format)));
    }

    // Only support 2D textures for PNG export
    if (m_desc.type != TextureType::TEXTURE_2D) {
        return Error("PNG export only supports 2D textures");
    }

    // Only export the first mip level
    if (m_desc.mipLevels > 1) {
        LOG(WARNING) << "PNG export will only save the first mip level of texture";
    }

    // Only export the first array slice
    if (m_desc.arraySize > 1) {
        LOG(WARNING) << "PNG export will only save the first array slice of texture";
    }

    // Use lodepng to encode and save the PNG
    std::vector<uint8_t> encodedPng;
    unsigned error = lodepng::encode(encodedPng, pngData, width, height, colorType, bitDepth);
    
    if (error) {
        return Error("LodePNG encoding error: " + std::string(lodepng_error_text(error)));
    }

    // Save to file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Error("Failed to open file for writing: " + path.string());
    }

    file.write(reinterpret_cast<const char*>(encodedPng.data()), encodedPng.size());
    file.close();

    if (!file.good()) {
        return Error("Failed to write PNG data to file: " + path.string());
    }

    return {};
}

#ifdef USE_KTX
Error Texture::SaveKTX2(const std::filesystem::path& path) const {
    // Only support certain texture types for KTX2 export
    ktx_uint32_t vkFormat = TextureFormatToVkFormat(m_desc.format);
    if (vkFormat == VkFormat::VK_FORMAT_UNDEFINED) {
        return Error("Unsupported texture format for KTX2 export: " + std::to_string(static_cast<int>(m_desc.format)));
    }
    
    // Create KTX2 texture create info
    ktxTextureCreateInfo createInfo = {};
    createInfo.glInternalformat = 0; // Not used for KTX2
    createInfo.vkFormat = vkFormat;
    createInfo.pDfd = nullptr; // Let KTX generate DFD from vkFormat
    createInfo.baseWidth = m_desc.width;
    createInfo.baseHeight = m_desc.height;
    createInfo.baseDepth = m_desc.depth;
    createInfo.numDimensions = (m_desc.depth > 1) ? 3 : (m_desc.height > 1) ? 2 : 1;
    createInfo.numLevels = m_desc.mipLevels;
    createInfo.numLayers = m_desc.arraySize;
    createInfo.numFaces = (m_desc.type == TextureType::TEXTURE_CUBE) ? 6 : 1;
    createInfo.isArray = (m_desc.type == TextureType::TEXTURE_2D_ARRAY) ? KTX_TRUE : KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;  // Never generate mipmaps
    
    // Create empty KTX2 texture
    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    
    if (result != KTX_SUCCESS) {
        return Error("Failed to create KTX2 texture (error code: " + std::to_string(result) + ")");
    }
    
    if (!texture) {
        return Error("Created KTX2 texture is null");
    }
    
    // Set the texture data using the KTX API
    result = ktxTexture_SetImageFromMemory((ktxTexture*)texture, 
                                           0,  // level (mip level 0)
                                           0,  // layer  
                                           0,  // faceSlice
                                           m_data.data(), 
                                           m_data.size());
    
    if (result != KTX_SUCCESS) {
        ktxTexture_Destroy((ktxTexture*)texture);
        return Error("Failed to set KTX texture data (error code: " + std::to_string(result) + ")");
    }
    
    // Write the KTX2 file
    result = ktxTexture_WriteToNamedFile((ktxTexture*)texture, path.string().c_str());
    
    // Clean up
    ktxTexture_Destroy((ktxTexture*)texture);
    
    if (result != KTX_SUCCESS) {
        return Error("Failed to write KTX2 file: " + path.string() + " (error code: " + std::to_string(result) + ")");
    }
    
    return {};
}

size_t Texture::GetMipOffset(int mipLevel) const {
    if (mipLevel >= m_desc.mipLevels) {
        throw std::out_of_range("Invalid mip level");
    }

    size_t offset = 0;
    for (int i = 0; i < mipLevel; ++i) {
        offset += GetMipSize(i);
    }
    return offset;
}

size_t Texture::GetMipSize(int mipLevel) const {
    if (mipLevel >= m_desc.mipLevels) {
        throw std::out_of_range("Invalid mip level");
    }

    uint32_t mipWidth = std::max(1u, m_desc.width >> mipLevel);
    uint32_t mipHeight = std::max(1u, m_desc.height >> mipLevel);
    uint32_t mipDepth = (m_desc.type == TextureType::TEXTURE_3D) ? std::max(1u, m_desc.depth >> mipLevel) : 1;
    uint32_t arraySize = (m_desc.type == TextureType::TEXTURE_2D_ARRAY || m_desc.type == TextureType::TEXTURE_CUBE) ? m_desc.arraySize : 1;

    return mipWidth * mipHeight * mipDepth * arraySize * GetPixelStride(m_desc.format);
}

const std::span<uint8_t const> Texture::GetData(int mipLevel) const {
    // Validate mip level
    if (mipLevel >= m_desc.mipLevels) {
        throw std::out_of_range("Invalid mip level");
    }

    // Calculate offset and size for the specified mip level
    size_t offset = GetMipOffset(mipLevel);
    size_t size = GetMipSize(mipLevel);

    return std::span(m_data).subspan(offset, size);
}

std::span<uint8_t> Texture::GetData(int mipLevel) {
    // Validate mip level
    if (mipLevel >= m_desc.mipLevels) {
        throw std::out_of_range("Invalid mip level");
    }

    // Calculate offset and size for the specified mip level
    size_t offset = GetMipOffset(mipLevel);
    size_t size = GetMipSize(mipLevel);

    return std::span(m_data).subspan(offset, size);
}

#else
Error Texture::SaveKTX2(const std::filesystem::path& path) const {
    return Error("KTX2 support not compiled in");
}
#endif

} // namespace okami
