#pragma once

#include "common.hpp"
#include "texture.hpp"

#include <glm/vec4.hpp>
#include <memory>
#include <typeindex>

namespace okami {
	struct DefaultMaterial {
	};

	struct LambertMaterial {
		TextureHandle m_colorTexture;
		TextureHandle m_normalTexture; // tangent-space normal map; may be null
		glm::vec4 m_colorTint{1.0f};
	};

	// Abstract base class for all backend material implementations.
	// Concrete types (e.g., OGLMaterial) are created and owned by the backend
	// material manager and are reference-counted through MaterialHandle.
	class IMaterial {
	public:
		virtual ~IMaterial() = default;
		virtual std::type_index GetType() const = 0;
	};

	// A MaterialHandle is a shared, reference-counted pointer to a backend
	// material implementation.  Lifetime is managed automatically; the material
	// is destroyed when the last handle is dropped.
	using MaterialHandle = std::shared_ptr<IMaterial>;

	// Factory interface implemented by the backend material manager for each
	// supported frontend material type T.
	template <typename T>
	class IMaterialManager {
	public:
		virtual ~IMaterialManager() = default;
		virtual MaterialHandle CreateMaterial(T material) = 0;
	};
}