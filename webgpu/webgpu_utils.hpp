#pragma once

#include "../renderer.hpp"

#include <webgpu.h>

namespace okami {
    struct WgpuRenderPassInfo {
		Camera m_camera;
		glm::ivec2 m_viewportSize;
		WGPURenderPassEncoder m_info;
	};

	class IWgpuRenderModule {
	public:
		virtual ~IWgpuRenderModule() = default;

		virtual Error Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) = 0;
	};
}