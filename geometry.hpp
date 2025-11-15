#pragma once

#include <vector>
#include <optional>
#include <span>
#include <filesystem>

#include "common.hpp"
#include "aabb.hpp"

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/common.hpp>

namespace okami {
    enum class AttributeType {
		Position,
		Normal,
		TexCoord,
		Color,
		Tangent,
		Bitangent,
		Unknown
		// Add more attribute types as needed
	};

	std::string_view AttributeTypeToString(AttributeType type);

	enum class MeshType {
		Static
	};

    enum class AccessorType {
        Scalar,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat3,
        Mat4
    };

    enum class AccessorComponentType {
        Double,
        Float,
        Int,
        UInt,
        Short,
        UShort,
        Byte,
        UByte
    };

	AccessorType GetAccessorType(AttributeType type);
	AccessorComponentType GetComponentType(AttributeType type);
	uint32_t GetSize(AccessorType type, AccessorComponentType componentType);
	uint32_t GetSize(AttributeType type);

	struct IndexInfo {
		AccessorComponentType m_type;
		int m_buffer;
		size_t m_count;
		size_t m_offset;

		uint32_t GetComponentSize() const;
        size_t GetTotalSize() const;
	};

	struct GeometryLoadParams {
	};

	void GenerateDefaultAttributeData(
    	std::span<uint8_t> buffer, 
    	AttributeType attrType);

	template <typename T>
    constexpr bool VerifyGeometryAttributeType(AttributeType attrType) {
        switch (attrType) {
            case AttributeType::Position:
                return std::is_same_v<T, glm::vec3>;
            case AttributeType::Normal:
                return std::is_same_v<T, glm::vec3>;
            case AttributeType::TexCoord:
                return std::is_same_v<T, glm::vec2>;
            case AttributeType::Color:
                return std::is_same_v<T, glm::vec4>;
            case AttributeType::Tangent:
                return std::is_same_v<T, glm::vec4>;
            case AttributeType::Bitangent:
                return std::is_same_v<T, glm::vec3>;
            default:
                return false;
        }
    }

    template <typename T>
    constexpr bool VerifyIndexType(AccessorComponentType componentType) {
        switch (componentType) {
            case AccessorComponentType::UByte:
                return std::is_same_v<T, uint8_t>;
            case AccessorComponentType::UShort:
                return std::is_same_v<T, uint16_t>;
            case AccessorComponentType::UInt:
                return std::is_same_v<T, uint32_t>;
            default:
                return false;
        }
    }

    template <typename T>
    struct GeometryViewIterator {
        std::conditional_t<std::is_const_v<T>, const uint8_t*, uint8_t*> m_data;
        uint32_t m_stride;

        T& operator*() const {
            return *reinterpret_cast<T*>(m_data);
        }

        GeometryViewIterator& operator++() {
            m_data += m_stride;
            return *this;
        }

        bool operator!=(const GeometryViewIterator& other) const {
            return m_data != other.m_data;
        }

        bool operator==(const GeometryViewIterator& other) const {
            return m_data == other.m_data;
        }
    };
    
    template <typename T>
    struct GeometryView {
        GeometryViewIterator<T> m_begin;
        GeometryViewIterator<T> m_end;

        GeometryViewIterator<T> begin() const {
            return m_begin;
        }

        GeometryViewIterator<T> end() const {
            return m_end;
        }
        
        T& operator[](size_t index) const {
            auto it = m_begin;
            it.m_data += index * it.m_stride;
            return *it;
        }
    };

    struct Attribute {
        AttributeType m_type;
        int m_buffer;
        size_t m_offset;
        uint32_t m_stride;

        uint32_t GetComponentSize() const;
        uint32_t GetStride() const;
    };

    struct GeometryPrimitiveDesc {
		std::unordered_map<AttributeType, Attribute> m_attributes;
		size_t m_vertexCount = 0;
		std::optional<IndexInfo> m_indices;
		MeshType m_type;
		AABB m_aabb;

		Attribute const* TryGetAttribute(AttributeType type) const;

		inline bool HasIndexBuffer() const {
			return m_indices.has_value();
		}
	};

    struct GeometryDesc {
		std::vector<GeometryPrimitiveDesc> m_primitives;
	};

    class Geometry {
	private:
        std::vector<std::vector<uint8_t>> m_buffers;
        GeometryDesc m_desc;

	public:
		Geometry() = default;
		OKAMI_NO_COPY(Geometry);
		OKAMI_MOVE(Geometry);

		inline std::span<std::vector<uint8_t> const> GetBuffers() const {
			return std::span(m_buffers);
		}

		inline GeometryDesc const& GetDesc() const {
			return m_desc;
		}

        inline std::span<GeometryPrimitiveDesc const> GetPrimitives() const {
            return std::span(m_desc.m_primitives);
        }

		inline size_t GetPrimitiveCount() const {
			return m_desc.m_primitives.size();
		}

		inline std::span<uint8_t const> GetRawVertexData(int buffer = 0) const {
			if (buffer < 0 || buffer >= m_buffers.size()) {
				throw std::out_of_range("Invalid buffer index");
			}
			return m_buffers[buffer];
		}

		inline std::span<uint8_t> GetRawVertexData(int buffer = 0) {
			if (buffer < 0 || buffer >= m_buffers.size()) {
				throw std::out_of_range("Invalid buffer index");
			}
			return m_buffers[buffer];
		}

        template <typename T>
        static Expected<GeometryView<T>> TryAccessIndexedStatic(
            std::conditional_t<std::is_const_v<T>, Geometry const&, Geometry&> data, 
            size_t primitiveIndex = 0) {

            OKAMI_UNEXPECTED_RETURN_IF(primitiveIndex >= data.m_desc.m_primitives.size(), 
                "Primitive index out of bounds"); 
            OKAMI_UNEXPECTED_RETURN_IF(!VerifyIndexType<T>(data.m_desc.m_primitives[primitiveIndex].m_indices->m_type), 
                "Type T does not match the index buffer component type");

            GeometryPrimitiveDesc const& primitive = data.m_desc.m_primitives[primitiveIndex];
            OKAMI_UNEXPECTED_RETURN_IF(primitive.m_indices == std::nullopt, "No index buffer found");

            // Get the index buffer data
            auto indexBufferData = data.GetRawVertexData(primitive.m_indices->m_buffer);
            auto indexBufferOffset = primitive.m_indices->m_offset;
            auto stride = primitive.m_indices->GetComponentSize();

            return GeometryView<T>(
                GeometryViewIterator<T>{
                    indexBufferData.data() + indexBufferOffset,
                    stride
                },
                GeometryViewIterator<T>{
                    indexBufferData.data() + indexBufferOffset + stride * primitive.m_indices->m_count,
                    stride
                }
            );
        }

        template <typename T>
        Expected<GeometryView<T>> TryAccessIndexed(size_t primitiveIndex = 0) const {
            static_assert(std::is_const_v<T>, "T must be const for const Geometry");
            return TryAccessIndexedStatic<T>(*this, primitiveIndex);
        }

        template <typename T>
        Expected<GeometryView<T>> TryAccessIndexed(size_t primitiveIndex = 0) {
            return TryAccessIndexedStatic<T>(*this, primitiveIndex);
        }

        template <typename T>
        static Expected<GeometryView<T>> TryAccessStatic(
            std::conditional_t<std::is_const_v<T>, Geometry const&, Geometry&> data, 
            AttributeType attrType, 
            size_t primitiveIndex = 0) {
            OKAMI_UNEXPECTED_RETURN_IF(!VerifyGeometryAttributeType<T>(attrType), 
                "Type T does not match the AttributeType");
            OKAMI_UNEXPECTED_RETURN_IF(primitiveIndex >= data.m_desc.m_primitives.size(), 
                "Primitive index out of bounds");

            const auto& primitive = data.m_desc.m_primitives[primitiveIndex];
            // Find the attribute for the given attribute type
            Attribute const* attribute = primitive.TryGetAttribute(attrType);
            OKAMI_UNEXPECTED_RETURN_IF(attribute == nullptr, 
                "AttributeType not found in primitive");

            // Get the buffer data
            auto bufferData = data.GetRawVertexData(attribute->m_buffer);

            auto bufferOffset = attribute->m_offset;
            auto stride = attribute->GetComponentSize();

            // Create a view of the buffer data
            return GeometryView<T>(
                GeometryViewIterator<T>{
                    bufferData.data() + bufferOffset,
                    stride
                },
                GeometryViewIterator<T>{
                    bufferData.data() + bufferOffset + stride * primitive.m_vertexCount,
                    stride
                }
			);
        }

		template <typename T>
		Expected<GeometryView<T>> TryAccess(AttributeType attrType, size_t primitiveIndex = 0) const {
			static_assert(std::is_const_v<T>, "T must be const for const Geometry");
            return TryAccessStatic<T>(*this, attrType, primitiveIndex);
		}

		template <typename T>
		Expected<GeometryView<T>> TryAccess(AttributeType attrType, size_t primitiveIndex = 0) {
			return TryAccessStatic<T>(*this, attrType, primitiveIndex);
		}

		static Expected<Geometry> LoadGLTF(std::filesystem::path const& path);

		using Desc = GeometryDesc;
        using LoadParams = GeometryLoadParams;
	};
}