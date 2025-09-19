#pragma once

#include <stdexcept>
#include <atomic>
#include <memory>

#include "engine.hpp"
#include "config.hpp"

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
		virtual void* GetNativeWindowHandle() = 0;
		virtual void* GetNativeDisplayType() = 0;
		virtual glm::ivec2 GetFramebufferSize() = 0;
		
		virtual ~INativeWindowProvider() = default;
	};

	struct DummyTriangleComponent {};

	/*struct Geometry {
        std::vector<GeometryMeshDesc> m_meshes;
		std::any m_privateData;

		using CreationData = RawGeometry;
	};

	struct StaticMeshComponent {
		ResHandle<Geometry> m_mesh;
		int m_meshIndex = 0;

		inline auto operator<=>(StaticMeshComponent const& other) const {
			return (std::pair(m_mesh.Ptr(), m_meshIndex) <=> std::pair(other.m_mesh.Ptr(), other.m_meshIndex));
		}
		inline bool operator==(StaticMeshComponent const& other) const {
			return m_mesh.Ptr() == other.m_mesh.Ptr() && m_meshIndex == other.m_meshIndex;
		}
	};*/

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

	/*struct SpriteComponent {
		ResHandle<Texture> m_texture;
		std::optional<glm::vec2> m_origin;
		std::optional<Rect> m_sourceRect;
		Color m_color = color::White;
		int m_layer = 0;
	};*/

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

    class IRenderer {
	public:
		virtual ~IRenderer() = default;

		virtual Error SaveToFile(const std::string& filename) = 0;
		virtual void SetHeadlessMode(bool headless) = 0;

		virtual void SetActiveCamera(entity_t e) = 0;
		virtual entity_t GetActiveCamera() const = 0;
	};
}
