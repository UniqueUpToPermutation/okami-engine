#include "webgpu_texture.hpp"
#include <glog/logging.h>

using namespace okami;

// Helper function to convert TextureFormat to WGPUTextureFormat
static WGPUTextureFormat ToWebGPUFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
            return WGPUTextureFormat_R8Unorm;
        case TextureFormat::RG8:
            return WGPUTextureFormat_RG8Unorm;
        case TextureFormat::RGB8:
            return WGPUTextureFormat_RGBA8Unorm; // WebGPU doesn't have RGB8, use RGBA8
        case TextureFormat::RGBA8:
            return WGPUTextureFormat_RGBA8Unorm;
        case TextureFormat::R32F:
            return WGPUTextureFormat_R32Float;
        case TextureFormat::RG32F:
            return WGPUTextureFormat_RG32Float;
        case TextureFormat::RGB32F:
            return WGPUTextureFormat_RGBA32Float; // WebGPU doesn't have RGB32F, use RGBA32F
        case TextureFormat::RGBA32F:
            return WGPUTextureFormat_RGBA32Float;
        default:
            return WGPUTextureFormat_RGBA8Unorm; // Fallback
    }
}

// Helper function to convert TextureType to WGPUTextureDimension
static WGPUTextureDimension ToWebGPUDimension(TextureType type) {
    switch (type) {
        case TextureType::TEXTURE_1D:
            return WGPUTextureDimension_1D;
        case TextureType::TEXTURE_2D:
        case TextureType::TEXTURE_2D_ARRAY:
        case TextureType::TEXTURE_CUBE:
            return WGPUTextureDimension_2D;
        case TextureType::TEXTURE_3D:
            return WGPUTextureDimension_3D;
        default:
            return WGPUTextureDimension_2D; // Fallback
    }
}

// Helper function to convert TextureType to WGPUTextureViewDimension
static WGPUTextureViewDimension ToWebGPUViewDimension(TextureType type) {
    switch (type) {
        case TextureType::TEXTURE_1D:
            return WGPUTextureViewDimension_1D;
        case TextureType::TEXTURE_2D:
            return WGPUTextureViewDimension_2D;
        case TextureType::TEXTURE_2D_ARRAY:
            return WGPUTextureViewDimension_2DArray;
        case TextureType::TEXTURE_3D:
            return WGPUTextureViewDimension_3D;
        case TextureType::TEXTURE_CUBE:
            return WGPUTextureViewDimension_Cube;
        default:
            return WGPUTextureViewDimension_2D; // Fallback
    }
}

Expected<std::pair<typename Texture::Desc, WebGpuTextureImpl>> 
    WebGpuTextureModule::CreateResource(Texture&& data, std::any userData) {
    
    // Extract device from user data
    if (!userData.has_value()) {
        return std::unexpected(Error("No WebGPU device provided in user data"));
    }
    
    WebGpuTextureModuleUserData sUserData;
    try {
        sUserData = std::any_cast<WebGpuTextureModuleUserData>(userData);
    } catch (const std::bad_any_cast&) {
        return std::unexpected(Error("Invalid user data type, expected WebGpuTextureModuleUserData"));
    }
    
    if (!sUserData.m_device) {
        return std::unexpected(Error("Invalid WebGPU device in user data"));
    }
    if (!sUserData.m_queue) {
        return std::unexpected(Error("Invalid WebGPU queue in user data"));
    }

    WGPUDevice device = sUserData.m_device;
    const auto& desc = data.GetDesc();
    const auto& textureData = data.GetData();
    
    // Convert texture format and dimensions
    WGPUTextureFormat wgpuFormat = ToWebGPUFormat(desc.format);
    WGPUTextureDimension wgpuDimension = ToWebGPUDimension(desc.type);
    WGPUTextureViewDimension wgpuViewDimension = ToWebGPUViewDimension(desc.type);
    
    // Create texture descriptor
    WGPUTextureDescriptor textureDesc = {};
    textureDesc.dimension = wgpuDimension;
    textureDesc.format = wgpuFormat;
    textureDesc.mipLevelCount = desc.mipLevels;
    textureDesc.sampleCount = 1;
    textureDesc.size = {
        desc.width,
        desc.height,
        std::max(desc.depth, desc.arraySize)
    };
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    
    // Create the texture
    WebGpuTextureImpl impl;
    impl.m_texture = WTexture(wgpuDeviceCreateTexture(device, &textureDesc));
    if (!impl.m_texture) {
        return std::unexpected(Error("Failed to create WebGPU texture"));
    }
    
    // Create texture view
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = wgpuFormat;
    viewDesc.dimension = wgpuViewDimension;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = desc.mipLevels;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = (desc.type == TextureType::TEXTURE_2D_ARRAY || desc.type == TextureType::TEXTURE_CUBE) 
                                ? desc.arraySize : 1;
    
    impl.m_view = WTextureView(wgpuTextureCreateView(impl.m_texture.get(), &viewDesc));
    if (!impl.m_view) {
        return std::unexpected(Error("Failed to create WebGPU texture view"));
    }
    
    // Create sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.addressModeW = WGPUAddressMode_Repeat;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = static_cast<float>(desc.mipLevels);
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;
    
    impl.m_sampler = WSampler(wgpuDeviceCreateSampler(device, &samplerDesc));
    if (!impl.m_sampler) {
        return std::unexpected(Error("Failed to create WebGPU sampler"));
    }
    
    // Upload texture data if provided
    if (!textureData.empty()) {
        WGPUQueue queue = sUserData.m_queue;
        
        // Calculate bytes per pixel
        uint32_t bytesPerPixel = GetPixelStride(desc.format);
        uint32_t bytesPerRow = desc.width * bytesPerPixel;
        uint32_t rowsPerImage = desc.height;
        
        WGPUImageCopyTexture imageCopyTexture = {};
        imageCopyTexture.texture = impl.m_texture.get();
        imageCopyTexture.mipLevel = 0;
        imageCopyTexture.origin = {0, 0, 0};
        imageCopyTexture.aspect = WGPUTextureAspect_All;
        
        WGPUTextureDataLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = bytesPerRow;
        dataLayout.rowsPerImage = rowsPerImage;
        
        WGPUExtent3D writeSize = {desc.width, desc.height, std::max(desc.depth, 1u)};
        
        wgpuQueueWriteTexture(queue, &imageCopyTexture, textureData.data(), 
                             textureData.size(), &dataLayout, &writeSize);
    }
    
    LOG(INFO) << "Created WebGPU texture: " << desc.width << "x" << desc.height 
              << ", format: " << static_cast<int>(desc.format)
              << ", mips: " << desc.mipLevels;
    
    return std::make_pair(desc, std::move(impl));
}