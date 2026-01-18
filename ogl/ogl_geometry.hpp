#pragma once

#include "ogl_utils.hpp"

#include "../geometry.hpp"
#include "../content.hpp"

namespace okami {
    struct PrimitiveImpl {
        GLVertexArray m_vao;
    };

    struct GeometryImpl {
        std::vector<PrimitiveImpl> m_meshes;
        std::vector<std::shared_ptr<GLBuffer>> m_buffers;
    };

    class OGLGeometryManager : 
        public ContentModule<Geometry, GeometryImpl> {
    private:
        glsl::VertexShaderInputInfo m_staticMeshVSInput;;
        
    public:
        Expected<std::pair<typename Geometry::Desc, GeometryImpl>> 
            CreateResource(Geometry&& data) override;
        void DestroyResourceImpl(GeometryImpl& impl) override;

        void SetStaticMeshVSInput(glsl::VertexShaderInputInfo info);
    };
}