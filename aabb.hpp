#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/common.hpp>

namespace okami {
    struct AABB {
		glm::vec3 m_min;
		glm::vec3 m_max;

		inline bool Contains(const glm::vec3& point) const {
			return (point.x >= m_min.x && point.x <= m_max.x &&
				point.y >= m_min.y && point.y <= m_max.y &&
				point.z >= m_min.z && point.z <= m_max.z);
		}

		inline bool Contains(const AABB& other) const {
			return (m_min.x <= other.m_min.x && m_max.x >= other.m_max.x &&
				m_min.y <= other.m_min.y && m_max.y >= other.m_max.y &&
				m_min.z <= other.m_min.z && m_max.z >= other.m_max.z);
		}
	};

	struct AABB2 {
		glm::vec2 m_min;
		glm::vec2 m_max;

		inline bool Contains(const glm::vec2& point) const {
			return (point.x >= m_min.x && point.x <= m_max.x &&
				point.y >= m_min.y && point.y <= m_max.y);
		}

		inline bool Contains(const AABB2& other) const {
			return (m_min.x <= other.m_min.x && m_max.x >= other.m_max.x &&
				m_min.y <= other.m_min.y && m_max.y >= other.m_max.y);
		}
	};

	inline AABB Union(const AABB& a, const AABB& b) {
		return AABB{
			glm::min(a.m_min, b.m_min),
			glm::max(a.m_max, b.m_max)
		};
	}

	inline AABB2 Union(const AABB2& a, const AABB2& b) {
		return AABB2{
			glm::min(a.m_min, b.m_min),
			glm::max(a.m_max, b.m_max)
		};
	}

	inline AABB Intersection(const AABB& a, const AABB& b) {
		return AABB{
			glm::max(a.m_min, b.m_min),
			glm::min(a.m_max, b.m_max)
		};
	}

	inline AABB2 Intersection(const AABB2& a, const AABB2& b) {
		return AABB2{
			glm::max(a.m_min, b.m_min),
			glm::min(a.m_max, b.m_max)
		};
	}

	inline bool Intersects(const AABB& a, const AABB& b) {
		return (a.m_min.x <= b.m_max.x && a.m_max.x >= b.m_min.x &&
			a.m_min.y <= b.m_max.y && a.m_max.y >= b.m_min.y &&
			a.m_min.z <= b.m_max.z && a.m_max.z >= b.m_min.z);
	}

	inline bool Intersects(const AABB2& a, const AABB2& b) {
		return (a.m_min.x <= b.m_max.x && a.m_max.x >= b.m_min.x &&
			a.m_min.y <= b.m_max.y && a.m_max.y >= b.m_min.y);
	}

	inline float Volume(const AABB& a) {
		return (a.m_max.x - a.m_min.x) * (a.m_max.y - a.m_min.y) * (a.m_max.z - a.m_min.z);
	}

	inline float Volume(const AABB2& a) {
		return (a.m_max.x - a.m_min.x) * (a.m_max.y - a.m_min.y);
	}

	inline float SurfaceArea(const AABB& a) {
		float dx = a.m_max.x - a.m_min.x;
		float dy = a.m_max.y - a.m_min.y;
		float dz = a.m_max.z - a.m_min.z;
		return 2.0f * (dx * dy + dy * dz + dz * dx);
	}

	inline float SurfaceArea(const AABB2& a) {
		float dx = a.m_max.x - a.m_min.x;
		float dy = a.m_max.y - a.m_min.y;
		return 2.0f * (dx + dy);
	}
}