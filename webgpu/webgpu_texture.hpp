#pragma once

#include "../content.hpp"
#include "../texture.hpp"

#include "webgpu_utils.hpp"

namespace okami {
    struct WebGpuTextureImpl {
        WTexture m_texture;
        WTextureView m_view;
        WSampler m_sampler;
    };

    struct WebGpuTextureModuleUserData {
        WGPUDevice m_device = nullptr;
        WGPUQueue m_queue = nullptr;
    };

    class WebGpuTextureModule : public ContentModule<Texture, WebGpuTextureImpl> {
    protected:
        Expected<std::pair<typename Texture::Desc, WebGpuTextureImpl>> 
            CreateResource(Texture&& data, std::any userData) override;
    };
}