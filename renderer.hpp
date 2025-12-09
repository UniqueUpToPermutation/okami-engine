#pragma once

#include <stdexcept>
#include <atomic>
#include <memory>
#include <any>

#include "engine.hpp"
#include "config.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "geometry.hpp"	

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace okami {
	using Color = glm::vec4;

	namespace color {
		constexpr Color White = Color(1.0f, 1.0f, 1.0f, 1.0f);
		constexpr Color Black = Color(0.0f, 0.0f, 0.0f, 1.0f);
		constexpr Color Red = Color(1.0f, 0.0f, 0.0f, 1.0f);
		constexpr Color Green = Color(0.0f, 1.0f, 0.0f, 1.0f);
		constexpr Color Blue = Color(0.0f, 0.0f, 1.0f, 1.0f);
		constexpr Color Yellow = Color(1.0f, 1.0f, 0.0f, 1.0f);
		constexpr Color Cyan = Color(0.0f, 1.0f, 1.0f, 1.0f);
		constexpr Color Magenta = Color(1.0f, 0.0f, 1.0f, 1.0f);
		constexpr Color Orange = Color(1.0f, 0.5f, 0.0f, 1.0f);
		constexpr Color Purple = Color(0.5f, 0.0f, 0.5f, 1.0f);
		constexpr Color Pink = Color(1.0f, 0.0f, 0.5f, 1.0f);
		constexpr Color Brown = Color(0.6f, 0.3f, 0.1f, 1.0f);
		constexpr Color Gray = Color(0.5f, 0.5f, 0.5f, 1.0f);
		constexpr Color LightGray = Color(0.8f, 0.8f, 0.8f, 1.0f);
		constexpr Color DarkGray = Color(0.3f, 0.3f, 0.3f, 1.0f);
		constexpr Color Transparent = Color(0.0f, 0.0f, 0.0f, 0.0f);
		constexpr Color CornflowerBlue = Color(0.39f, 0.58f, 0.93f, 1.0f);
	};

	class INativeWindowProvider {
	public:
		virtual void* GetNativeWindowHandle() const = 0;
		virtual void* GetNativeDisplayType() const = 0;
		virtual glm::ivec2 GetFramebufferSize() const = 0;
		
		virtual ~INativeWindowProvider() = default;
	};

	typedef void (*GL_GLADapiproc)(void);
	typedef GL_GLADapiproc (*GL_GLADloadfunc)(const char *name);

	// An interface to provide OpenGL context management
	class IGLProvider {
	public:
		virtual GL_GLADloadfunc GetGLLoaderFunction() const = 0;
		virtual void NotifyNeedGLContext() = 0;
		virtual void SwapBuffers() = 0;
		virtual void SetSwapInterval(int interval) = 0;
		virtual glm::ivec2 GetFramebufferSize() const = 0;
		virtual ~IGLProvider() = default;
	};

	struct DummyTriangleComponent {};

	struct StaticMeshComponent {
		ResHandle<Geometry> m_geometry;
	};

	struct Rect {
		glm::vec2 m_position = glm::vec2(0.0f, 0.0f);
		glm::vec2 m_size = glm::vec2(0.0f, 0.0f);

		inline glm::vec2 GetMin() const {
			return m_position;
		}

		inline glm::vec2 GetMax() const {
			return m_position + m_size;
		}

		inline glm::vec2 GetSize() const {
			return m_size;
		}
	};

	struct SpriteComponent {
		ResHandle<Texture> m_texture;
		std::optional<glm::vec2> m_origin;
		std::optional<Rect> m_sourceRect;
		Color m_color = color::White;
	};

	struct RendererConfig {
		int bufferCount = 2;
		int syncInterval = 1; // VSync enabled

		OKAMI_CONFIG(renderer) {
			OKAMI_CONFIG_FIELD(bufferCount);
			OKAMI_CONFIG_FIELD(syncInterval);
		}
	};

	struct WindowConfig {
		int backbufferWidth = 1280;
		int backbufferHeight = 720;
		std::string windowTitle = "Okami Engine";
		bool fullscreen = false;

		OKAMI_CONFIG(window) {
			OKAMI_CONFIG_FIELD(backbufferWidth);
			OKAMI_CONFIG_FIELD(backbufferHeight);
			OKAMI_CONFIG_FIELD(windowTitle);
			OKAMI_CONFIG_FIELD(fullscreen);
		}
	};

	struct RenderPassInfo {
		Camera m_camera;
		glm::ivec2 m_viewportSize;
		int m_target;
		std::any m_userData;
	};

	class IRenderModule {
	public:
		virtual ~IRenderModule() = default;

		virtual Error Pass(Time const& time, InterfaceCollection& ic, RenderPassInfo info) = 0;
	};

	struct RendererParams {
		bool m_headlessRenderToFile = true;
		std::string m_headlessOutputFileStem = "output";
		std::string m_headlessRenderOutputDir = "renders"; 
	};

    class IRenderer {
	public:
		virtual ~IRenderer() = default;

		virtual void SetActiveCamera(entity_t e) = 0;
		virtual entity_t GetActiveCamera() const = 0;
	};
}
