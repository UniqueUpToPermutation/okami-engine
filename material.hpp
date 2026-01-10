#pragma once

#include "common.hpp"
#include "content.hpp"

namespace okami {
    struct Material {
        std::type_index m_materialType;
        //material_id_t m_materialID = kInvalidMaterialID;
        std::atomic<int> m_refCount{0};
    };

    struct MaterialHandle {
    private:
        Material* m_material = nullptr;

    public:
        inline MaterialHandle() = default;
		inline MaterialHandle(Material* material) : m_material(material) {
			if (m_material) {
				m_material->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline MaterialHandle(const MaterialHandle& other) : m_material(other.m_material) {
			if (m_material) {
				m_material->m_refCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
		inline MaterialHandle& operator=(const MaterialHandle& other) {
			if (this != &other) {
				if (m_material) {
					m_material->m_refCount.fetch_sub(1, std::memory_order_relaxed);
				}
				m_material = other.m_material;
				if (m_material) {
					m_material->m_refCount.fetch_add(1, std::memory_order_relaxed);
				}
			}
			return *this;
		}
		inline ~MaterialHandle() {
			if (m_material) {
				m_material->m_refCount.fetch_sub(1, std::memory_order_relaxed);
			}
		}
    };

    template <typename T>
    class IMaterialManager {
    public:
        virtual ~IMaterialManager() = default;
        virtual MaterialHandle CreateMaterial(T material) = 0;
    };
}