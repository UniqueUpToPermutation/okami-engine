#pragma once

#include "../renderer.hpp"

#include <webgpu.h>
#include <glm/glm.hpp>
#include <string>

namespace okami {
    struct WgpuRenderPassInfo {
		Camera m_camera;
		glm::ivec2 m_viewportSize;
		WGPURenderPassEncoder m_info;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projMatrix;
	};

	class IWgpuRenderModule {
	public:
		virtual ~IWgpuRenderModule() = default;

		virtual Error Pass(Time const& time, ModuleInterface& mi, WgpuRenderPassInfo info) = 0;
	};

	// Utility function to load WGSL shader files
	std::string LoadShaderFile(const std::string& filename);
}