#include "ogl_sprite.hpp"
#include "../paths.hpp"

#include <glog/logging.h>
#include <glad/gl.h>
#include <algorithm>

using namespace okami;

Error OGLSpriteRenderer::RegisterImpl(InterfaceCollection& interfaces) {
    return {};
}

Error OGLSpriteRenderer::StartupImpl(InitContext const& context) {
    auto* cache = context.m_interfaces.Query<IGLShaderCache>();
    OKAMI_ERROR_RETURN_IF(!cache, "IGLShaderCache interface not available for OGLSpriteRenderer");

    m_sceneGlobalsProvider = context.m_interfaces.Query<IOGLSceneGlobalsProvider>();
    OKAMI_ERROR_RETURN_IF(!m_sceneGlobalsProvider, "IOGLSceneGlobalsProvider interface not available for OGLSpriteRenderer");

    // Create shader program with vertex, geometry, and fragment shaders
    auto program = CreateProgram(ProgramShaderPaths{
        .m_vertex = GetGLSLShaderPath("sprite.vs"),
        .m_fragment = GetGLSLShaderPath("sprite.fs"),
        .m_geometry = GetGLSLShaderPath("sprite.gs"),
    }, *cache);
    OKAMI_ERROR_RETURN(program);

    m_program = std::move(*program);
    
    // Create instance VBO for sprite data
    auto buffer = UploadVertexBuffer<glsl::SpriteInstance>::Create(
        glsl::__get_vs_input_infoSpriteInstance(), 1);
    OKAMI_ERROR_RETURN(buffer);
    m_instanceBuffer = std::move(*buffer);

    Error err;
    glUseProgram(m_program.get());
    err += GET_GL_ERROR();
    err += AssignTextureBindingPoint(m_program, "u_texture", TextureBindingPoints::SpriteTexture);
    OKAMI_ERROR_RETURN(err);

    LOG(INFO) << "OGL Sprite Renderer initialized successfully";
    return {};
}

void OGLSpriteRenderer::ShutdownImpl(InitContext const& context) {
    // OpenGL resources are automatically cleaned up by RAII wrappers
}

Error OGLSpriteRenderer::Pass(entt::registry const& registry, OGLPass const& pass) {
    Error err;

    // Collect all sprites and their transforms
    std::vector<std::pair<ResHandle<Texture>, glsl::SpriteInstance>> spriteInstances;
    
    registry.view<SpriteComponent, Transform>().each([&](entity_t entity, const SpriteComponent& sprite, const Transform& transform) {
        // Skip sprites without textures
        if (!sprite.m_texture || !sprite.m_texture.IsLoaded()) {
            return;
        }
        
        spriteInstances.emplace_back(std::make_pair(sprite.m_texture, CreateSpriteInstance(sprite, transform)));
    });

    // Sort sprites back-to-front for proper alpha blending
    std::sort(spriteInstances.begin(), spriteInstances.end(),
        [this](const auto& a, const auto& b) {
            if (a.second.a_position.z == b.second.a_position.z) {
                return a.first < b.first; // Break ties by texture handle
            }
            return a.second.a_position.z > b.second.a_position.z;
        });

    // Resize if necessary
    m_instanceBuffer.Reserve(spriteInstances.size());

    // Set up rendering state
    glUseProgram(m_program.get()); 
    err += GET_GL_ERROR();

    glBindVertexArray(m_instanceBuffer.GetVertexArray()); 
    err += GET_GL_ERROR();

    {
        auto map = m_instanceBuffer.Map();
        OKAMI_ERROR_RETURN_IF(!map, "Failed to map instance buffer for sprite rendering");
        for (size_t i = 0; i < spriteInstances.size(); ++i) {
            (*map)[i] = spriteInstances[i].second;
        }
    }

    // Set uniforms
    err += m_sceneGlobalsProvider->GetSceneGlobalsBuffer().Bind(BufferBindingPoints::SceneGlobals);

    // Enable blending for sprites
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Group sprites by texture to minimize texture binding
    ResHandle<Texture> currentTexture;
    uint32_t batchStart = 0;

    if (spriteInstances.empty()) {
        return {};
    }
    
    for (uint32_t i = 0; i <= spriteInstances.size(); ++i) {
        ResHandle<Texture> spriteTexture = spriteInstances[i < spriteInstances.size() ? i : 0].first;
        
        // Check if we need to flush the current batch
        if (i == spriteInstances.size() || spriteTexture.Ptr() != currentTexture.Ptr()) {
            if (currentTexture && currentTexture.IsLoaded() && i > batchStart) {
                // TODO: Bind texture here when texture system is integrated
                // For now, render without texture binding
                if (currentTexture.IsLoaded()) {
                    auto& tex = m_textureManager->GetImpl(currentTexture)->m_texture;
                    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(TextureBindingPoints::SpriteTexture));
                    glBindTexture(GL_TEXTURE_2D, tex.get());
                    err += GET_GL_ERROR();
                }

                uint32_t instanceCount = i - batchStart;
                // Draw points, geometry shader will expand them to quads
                glDrawArrays(GL_POINTS, batchStart, instanceCount);
                err += GET_GL_ERROR();
            }
            
            currentTexture = spriteTexture;
            batchStart = i;
        }
    }
    
    return {};
}

std::string OGLSpriteRenderer::GetName() const {
    return "OpenGL Sprite Renderer";
}

OGLSpriteRenderer::OGLSpriteRenderer(OGLTextureManager* textureManager) :
    m_textureManager(textureManager) {
}

glsl::SpriteInstance OGLSpriteRenderer::CreateSpriteInstance(const SpriteComponent& sprite, const Transform& transform) const {
    glsl::SpriteInstance instance;
    
    // Get position from transform
    instance.a_position = transform.TransformPoint(glm::vec3(0.0f));

    // Get rotation from transform (assuming 2D rotation around Z axis)
    instance.a_rotation = std::atan2(transform.m_rotation.z, transform.m_rotation.w); // Z-axis rotation for 2D sprites

    // Get size from sprite or use default
    if (sprite.m_texture && sprite.m_texture.IsLoaded()) {
        const TextureDesc& texDesc = sprite.m_texture.GetDesc();
        instance.a_size = glm::vec2(
            texDesc.width * transform.m_scaleShear[0][0], 
            texDesc.height * transform.m_scaleShear[1][1]
        );
    } else {
        instance.a_size = glm::vec2(transform.m_scaleShear[0][0], transform.m_scaleShear[1][1]);
    }
    
    // Set sprite rectangle (UV coordinates)
    if (sprite.m_sourceRect) {
        const Rect& rect = *sprite.m_sourceRect;
        instance.a_spriteRect = glm::vec4(rect.m_position, rect.m_size);
    } else {
        // Use full texture
        instance.a_spriteRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    
    // Set color
    instance.a_color = sprite.m_color;

    return instance;
}
