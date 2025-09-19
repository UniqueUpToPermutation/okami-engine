#include "geometry.hpp"
#include <tiny_gltf.h>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <glog/logging.h>
#include <cctype>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <numeric>

using namespace okami;

AccessorType okami::GetAccessorType(AttributeType type) {
    switch (type) {
        case AttributeType::Position: return AccessorType::Vec3;
        case AttributeType::Normal: return AccessorType::Vec3;
        case AttributeType::TexCoord: return AccessorType::Vec2;
        case AttributeType::Color: return AccessorType::Vec4;
        case AttributeType::Tangent: return AccessorType::Vec4;
        case AttributeType::Bitangent: return AccessorType::Vec3;
        default: throw std::runtime_error("Not implemented!");
    }
}

AccessorComponentType okami::GetComponentType(AttributeType type) {
    switch (type) {
        case AttributeType::Position: return AccessorComponentType::Float;
        case AttributeType::Normal: return AccessorComponentType::Float;
        case AttributeType::TexCoord: return AccessorComponentType::Float;
        case AttributeType::Color: return AccessorComponentType::Float;
        case AttributeType::Tangent: return AccessorComponentType::Float;
        case AttributeType::Bitangent: return AccessorComponentType::Float;
        default: throw std::runtime_error("Not implemented!");
    }
}

uint32_t okami::GetStride(AccessorType type, AccessorComponentType componentType) {
    uint32_t elementLength = 0;

    switch (componentType) {
        case AccessorComponentType::Double: elementLength = sizeof(double); break;
        case AccessorComponentType::Float: elementLength = sizeof(float); break;
        case AccessorComponentType::Int: elementLength = sizeof(int); break;
        case AccessorComponentType::UInt: elementLength = sizeof(unsigned int); break;
        case AccessorComponentType::Short: elementLength = sizeof(short); break;
        case AccessorComponentType::UShort: elementLength = sizeof(unsigned short); break;
        case AccessorComponentType::Byte: elementLength = sizeof(char); break;
        case AccessorComponentType::UByte: elementLength = sizeof(unsigned char); break;
    }

    uint32_t attributeCount = 0;
    switch (type) {
        case AccessorType::Scalar: attributeCount = 1; break;
        case AccessorType::Vec2: attributeCount = 2; break;
        case AccessorType::Vec3: attributeCount = 3; break;
        case AccessorType::Vec4: attributeCount = 4; break;
        case AccessorType::Mat2: attributeCount = 2 * 2; break;
        case AccessorType::Mat3: attributeCount = 3 * 3; break;
        case AccessorType::Mat4: attributeCount = 4 * 4; break;
    }

    return elementLength * attributeCount;
}

Attribute const* GeometryMeshDesc::TryGetAttribute(AttributeType type) const {
    auto it = std::find_if(m_attributes.begin(), m_attributes.end(),
        [type](const Attribute& accessor) {
            return accessor.m_type == type;
        });
    if (it != m_attributes.end()) {
        return &*it;
    } else {
        return nullptr;
    }
}

size_t GeometryMeshDesc::GetVertexByteSize() const {
    size_t size = 0;
    for (const auto& attr : m_attributes) {
        size += attr.GetStride() * m_vertexCount;
    }
    return size;
}

size_t GeometryMeshDesc::GetIndexByteSize() const {
    if (m_indices) {
        return m_indices->GetStride() * m_indices->m_count;
    }
    return 0;
}

uint32_t okami::GetStride(AttributeType type) {
    return GetStride(GetAccessorType(type), GetComponentType(type));
}

uint32_t Attribute::GetStride() const {
    return okami::GetStride(m_type);
}

uint32_t IndexInfo::GetStride() const {
    return okami::GetStride(AccessorType::Scalar, m_type);
}

void okami::GenerateDefaultAttributeData(
    std::span<uint8_t> buffer, 
    AttributeType attrType) {
    auto stride = GetStride(attrType);
        
    switch (attrType) {
        case AttributeType::Position: {
            std::span<glm::vec3> dst(reinterpret_cast<glm::vec3*>(buffer.data()), 
                buffer.size() / sizeof(glm::vec3));
            for (auto& val : dst) {
                val = {0.0f, 0.0f, 0.0f};
            }
            break;
        }
        case AttributeType::Normal: {
            // Default normal: (0, 0, 1) - pointing up
            std::span<glm::vec3> dst(reinterpret_cast<glm::vec3*>(buffer.data()), 
                buffer.size() / sizeof(glm::vec3));
            for (auto& val : dst) {
                val = {0.0f, 0.0f, 1.0f};
            }
            break;
        }
        case AttributeType::TexCoord: {
            // Default UV: (0, 0)
            std::span<glm::vec2> dst(reinterpret_cast<glm::vec2*>(buffer.data()), 
                buffer.size() / sizeof(glm::vec2));
            for (auto& val : dst) {
                val = {0.0f, 0.0f};
            }
            break;
        }
        case AttributeType::Color: {
            // Default color: white (1, 1, 1, 1)
            std::span<glm::vec4> dst(reinterpret_cast<glm::vec4*>(buffer.data()), 
                buffer.size() / sizeof(glm::vec4));
            for (auto& val : dst) {
                val = {1.0f, 1.0f, 1.0f, 1.0f};
            }
            break;
        }
        case AttributeType::Tangent: {
            // Default tangent: (1, 0, 0, 1) - pointing right with handedness
            std::span<glm::vec4> dst(reinterpret_cast<glm::vec4*>(buffer.data()), 
                buffer.size() / sizeof(glm::vec4));
            for (auto& val : dst) {
                val = {1.0f, 0.0f, 0.0f, 1.0f};
            }
            break;
        }
        case AttributeType::Bitangent: {
            // Default bitangent: (0, 1, 0) - pointing forward
            std::span<glm::vec3> dst(reinterpret_cast<glm::vec3*>(buffer.data()), 
                buffer.size() / sizeof(glm::vec3));
            for (auto& val : dst) {
                val = {0.0f, 1.0f, 0.0f};
            }
            break;
        }
        default:
        {
            std::memset(buffer.data(), 0, buffer.size());
        }
    }
}

// Helper functions for GLTF loading
namespace {
    AccessorComponentType ConvertComponentType(int gltfComponentType) {
        switch (gltfComponentType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: return AccessorComponentType::Byte;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return AccessorComponentType::UByte;
            case TINYGLTF_COMPONENT_TYPE_SHORT: return AccessorComponentType::Short;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return AccessorComponentType::UShort;
            case TINYGLTF_COMPONENT_TYPE_INT: return AccessorComponentType::Int;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return AccessorComponentType::UInt;
            case TINYGLTF_COMPONENT_TYPE_FLOAT: return AccessorComponentType::Float;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE: return AccessorComponentType::Double;
            default: return AccessorComponentType::Float; // fallback
        }
    }

    AccessorType ConvertAccessorType(int gltfType) {
        switch (gltfType) {
            case TINYGLTF_TYPE_SCALAR: return AccessorType::Scalar;
            case TINYGLTF_TYPE_VEC2: return AccessorType::Vec2;
            case TINYGLTF_TYPE_VEC3: return AccessorType::Vec3;
            case TINYGLTF_TYPE_VEC4: return AccessorType::Vec4;
            case TINYGLTF_TYPE_MAT2: return AccessorType::Mat2;
            case TINYGLTF_TYPE_MAT3: return AccessorType::Mat3;
            case TINYGLTF_TYPE_MAT4: return AccessorType::Mat4;
            default: return AccessorType::Vec3; // fallback
        }
    }

    AttributeType MapGLTFAttributeName(const std::string& name) {
        if (name == "POSITION") return AttributeType::Position;
        if (name == "NORMAL") return AttributeType::Normal;
        if (name == "TEXCOORD_0") return AttributeType::TexCoord;
        if (name == "COLOR_0") return AttributeType::Color;
        if (name == "TANGENT") return AttributeType::Tangent;
        // Note: GLTF doesn't typically have BITANGENT, it's computed from normal and tangent
        return AttributeType::Unknown; // fallback - will be handled as unsupported
    }
}