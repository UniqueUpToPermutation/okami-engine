#pragma once

#include "ogl_utils.hpp"

#include "../geometry.hpp"
#include "../content.hpp"

namespace okami {
    struct PrimitiveImpl {
        GLVertexArray m_vao;
        std::optional<int> m_indexBufferIndex;
        size_t m_indexBufferOffset = 0;
    };

    struct GeometryImpl {
        std::vector<PrimitiveImpl> m_meshes;
        std::vector<std::shared_ptr<GLBuffer>> m_buffers;
    };

    class OGLGeometryManager : 
        public ContentModule<Geometry, GeometryImpl> {
    private:
        glsl::vs_meta_func_t m_staticMeshMetaFunc;
        
    public:
        Expected<std::pair<typename Geometry::Desc, GeometryImpl>> 
            CreateResource(Geometry&& data, std::any userData) override;

        void SetStaticMeshMetaFunc(glsl::vs_meta_func_t func);
    };
}