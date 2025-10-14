#include "ogl_sprite.hpp"
#include "../paths.hpp"

#include <glog/logging.h>
#include <glad/gl.h>
#include <algorithm>

using namespace okami;

Error OGLSpriteRenderer::RegisterImpl(ModuleInterface& mi) {
    return {};
}

Error OGLSpriteRenderer::StartupImpl(ModuleInterface& mi) {
    auto* cache = mi.m_interfaces.Query<IGLShaderCache>();
    if (!cache) {
        return Error("IGLShaderCache interface not available for OGLSpriteRenderer");
    }

    m_transformView = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!m_transformView) {
        return Error("IComponentView<Transform> interface not available for OGLSpriteRenderer");
    }

    // Create shader program with vertex, geometry, and fragment shaders
    auto program = CreateProgram(ProgramShaderPaths{
        .m_vertex = GetGLSLShaderPath("sprite.vs"),
        .m_fragment = GetGLSLShaderPath("sprite.fs")
    }, *cache);
    OKAMI_ERROR_RETURN(program);

    m_program = std::move(*program);

    // Create and setup VAO
    GLuint vaoId;
    glGenVertexArrays(1, &vaoId);
    m_vao = GLVertexArray(vaoId);
    
    // Create instance VBO for sprite data
    GLuint vboId;
    glGenBuffers(1, &vboId);
    m_instanceVBO = GLBuffer(vboId);
    
    // Setup VAO with sprite instance attributes
    glBindVertexArray(m_vao.get());
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO.get());
    
    // Reserve space for maximum sprites (will be updated each frame)
    glBufferData(GL_ARRAY_BUFFER, MAX_SPRITES * sizeof(glsl::SpriteInstance), nullptr, GL_DYNAMIC_DRAW);
    
    // Setup vertex attributes after buffer is bound
    glsl::__create_vertex_array_SpriteInstance();
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Get uniform locations
    Error uniformErrs;
    u_viewProj = GetUniformLocation(m_program, "u_viewProj", uniformErrs);
    u_texture = GetUniformLocation(m_program, "u_texture", uniformErrs);
    OKAMI_ERROR_RETURN(uniformErrs);

    LOG(INFO) << "OGL Sprite Renderer initialized successfully";
    return {};
}

void OGLSpriteRenderer::ShutdownImpl(ModuleInterface& mi) {
    // OpenGL resources are automatically cleaned up by RAII wrappers
}

Error OGLSpriteRenderer::ProcessFrameImpl(Time const&, ModuleInterface& mi) {
    return {};
}

Error OGLSpriteRenderer::MergeImpl() {
    return {};
}

Error OGLSpriteRenderer::Pass(OGLPass const& pass) {
    // Collect all sprites and their transforms
    std::vector<std::pair<entity_t, glsl::SpriteInstance>> spriteInstances;
    
    m_storage->ForEach([&](entity_t entity, const SpriteComponent& sprite) {
        // Skip sprites without textures
        if (!sprite.m_texture || !sprite.m_texture.IsLoaded()) {
            return;
        }
        
        Transform transform = m_transformView->GetOr(entity, Transform::Identity());
        glsl::SpriteInstance instance = CreateSpriteInstance(sprite, transform);
        
        spriteInstances.emplace_back(entity, instance);
    });
    
    if (spriteInstances.empty()) {
        return {};
    }
    
    // Sort sprites back-to-front for proper alpha blending
    SortSpritesBackToFront(spriteInstances);
    
    // Limit to maximum sprites per batch
    if (spriteInstances.size() > MAX_SPRITES) {
        spriteInstances.resize(MAX_SPRITES);
        LOG(WARNING) << "Too many sprites (" << spriteInstances.size() << "), limiting to " << MAX_SPRITES;
    }
    
    // Upload sprite instance data
    std::vector<glsl::SpriteInstance> instanceData;
    instanceData.reserve(spriteInstances.size());
    for (const auto& pair : spriteInstances) {
        instanceData.push_back(pair.second);
    }
    
    // Set up rendering state
    glUseProgram(m_program.get());
    glBindVertexArray(m_vao.get());
    
    // Upload sprite instance data (VAO binding also binds the associated buffer)
    glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(glsl::SpriteInstance), instanceData.data());
    
    // Set uniforms
    glUniformMatrix4fv(u_viewProj, 1, GL_FALSE, &pass.m_viewProjection[0][0]);
    glUniform1i(u_texture, 0); // Texture unit 0
    
    // Enable blending for sprites
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Group sprites by texture to minimize texture binding
    ResHandle<Texture> currentTexture;
    uint32_t batchStart = 0;
    
    for (uint32_t i = 0; i <= static_cast<uint32_t>(spriteInstances.size()); ++i) {
        ResHandle<Texture> spriteTexture;
        if (i < spriteInstances.size()) {
            // Find the sprite component for this instance
            entity_t entity = spriteInstances[i].first;
            if (auto* spriteComp = m_storage->TryGet(entity)) {
                spriteTexture = spriteComp->m_texture;
            }
        }
        
        // Check if we need to flush the current batch
        if (i == spriteInstances.size() || spriteTexture.Ptr() != currentTexture.Ptr()) {
            if (currentTexture && currentTexture.IsLoaded() && i > batchStart) {
                // TODO: Bind texture here when texture system is integrated
                // For now, render without texture binding
                if (currentTexture.IsLoaded()) {
                    auto& tex = m_textureManager->GetImpl(currentTexture)->m_texture;
                    glBindTexture(GL_TEXTURE_2D, tex.get());
                }

                uint32_t instanceCount = i - batchStart;
                // Draw 4 vertices per sprite instance using instanced rendering
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4 * instanceCount);
            }
            
            currentTexture = spriteTexture;
            batchStart = i;
        }
    }
    
    // Cleanup
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    
    return {};
}

std::string_view OGLSpriteRenderer::GetName() const {
    return "OpenGL Sprite Renderer";
}

OGLSpriteRenderer::OGLSpriteRenderer(OGLTextureManager* textureManager) :
    m_textureManager(textureManager) {
    m_storage = CreateChild<StorageModule<SpriteComponent>>();
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

void OGLSpriteRenderer::SortSpritesBackToFront(
    std::vector<std::pair<entity_t, glsl::SpriteInstance>>& sprites) const {
    std::sort(sprites.begin(), sprites.end(), 
        [this](const auto& a, const auto& b) {
            // Get Z positions from transforms for depth sorting
            Transform transformA = m_transformView->GetOr(a.first, Transform::Identity());
            Transform transformB = m_transformView->GetOr(b.first, Transform::Identity());
            
            glm::vec3 posA = transformA.TransformPoint(glm::vec3(0.0f));
            glm::vec3 posB = transformB.TransformPoint(glm::vec3(0.0f));
            
            // Sort back-to-front (higher Z values first)
            return posA.z > posB.z;
        });
}