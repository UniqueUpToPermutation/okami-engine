#include "bgfx_sprite.hpp"

#include "bgfx_util.hpp"

#include "../transform.hpp"

#include <bx/math.h>

using namespace okami;

Error BgfxSpriteModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error BgfxSpriteModule::StartupImpl(ModuleInterface&) {
    auto vs = LoadBgfxShader("sprite_vs.sc");
    auto fs = LoadBgfxShader("sprite_fs.sc");

    OKAMI_ERROR_RETURN(vs);
    OKAMI_ERROR_RETURN(fs);

    auto program = bgfx::createProgram(*vs, *fs, true);
    if (!bgfx::isValid(program)) {
        return Error("Failed to create sprite shader program");
    }

    m_program = AutoHandle(program);

    // Initialize vertex layout
    m_vertexLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)  // vec3 position
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)    // vec4 color0 (world pos + size)
        .add(bgfx::Attrib::Color1, 4, bgfx::AttribType::Float)    // vec4 color1 (sprite color)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float) // vec2 texcoord0
        .end();

    return {};
}

void BgfxSpriteModule::ShutdownImpl(ModuleInterface&) {
    m_program.Reset();
}

Error BgfxSpriteModule::ProcessFrameImpl(Time const&, ModuleInterface& mi) {
    return {};
}

Error BgfxSpriteModule::MergeImpl() {
    return {};
}
    
std::string_view BgfxSpriteModule::GetName() const {
    return "BGFX Sprite Module";
}

Error BgfxSpriteModule::Pass(Time const& time, ModuleInterface& mi, RenderPassInfo info) {
    auto transforms = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!transforms) {
        return Error("No IComponentView<Transform> available in BgfxSpriteModule");
    }

    if (!m_storage->IsEmpty() && bgfx::isValid(m_program)) {
        m_storage->ForEach([this, time, transforms, info](entity_t e, SpriteComponent const& sprite) {
            auto transform = transforms->GetOr(e, Transform::Identity()).AsMatrix();

            // Get texture from texture manager
            bgfx::TextureHandle textureHandle = BGFX_INVALID_HANDLE;
            if (sprite.m_texture.IsLoaded() && m_textureManager) {
                textureHandle = m_textureManager->GetBgfxTextureHandle(sprite.m_texture);
            }

            // Set texture sampler
            if (bgfx::isValid(textureHandle)) {
                static bgfx::UniformHandle s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
                bgfx::setTexture(0, s_texColor, textureHandle);
            }

            // Calculate sprite properties
            glm::vec2 size(100.0f, 100.0f); // Default size
            if (sprite.m_texture.IsLoaded()) {
                const auto& textureDesc = *sprite.m_texture;
                size.x = static_cast<float>(textureDesc.width);
                size.y = static_cast<float>(textureDesc.height);
            }

            glm::vec2 origin = sprite.m_origin.value_or(glm::vec2(0.0f, 0.0f));
            
            // Use source rect if specified
            if (sprite.m_sourceRect.has_value()) {
                size = sprite.m_sourceRect->GetSize();
            }

            // Extract world position from transform
            glm::vec3 worldPos = glm::vec3(transform[3]);

            // Create vertex data for a quad (6 vertices, 2 triangles)
            struct SpriteVertex {
                glm::vec3 position; // xyz = local vertex position
                glm::vec4 color0;   // xy = world position, zw = size
                glm::vec4 color1;   // rgba = color  
                glm::vec2 texcoord0; // uv coordinates
            };

            SpriteVertex vertices[6];
            
            // Define quad vertices in local space (origin at 0,0)
            // Triangle 1: (0,0), (1,0), (0,1)
            // Triangle 2: (1,0), (1,1), (0,1)
            
            vertices[0] = { {-origin.x/size.x, -origin.y/size.y, 0.0f}, {worldPos.x, worldPos.y, size.x, size.y}, sprite.m_color, {0.0f, 0.0f} };
            vertices[1] = { {(size.x-origin.x)/size.x, -origin.y/size.y, 0.0f}, {worldPos.x, worldPos.y, size.x, size.y}, sprite.m_color, {1.0f, 0.0f} };
            vertices[2] = { {-origin.x/size.x, (size.y-origin.y)/size.y, 0.0f}, {worldPos.x, worldPos.y, size.x, size.y}, sprite.m_color, {0.0f, 1.0f} };
            
            vertices[3] = { {(size.x-origin.x)/size.x, -origin.y/size.y, 0.0f}, {worldPos.x, worldPos.y, size.x, size.y}, sprite.m_color, {1.0f, 0.0f} };
            vertices[4] = { {(size.x-origin.x)/size.x, (size.y-origin.y)/size.y, 0.0f}, {worldPos.x, worldPos.y, size.x, size.y}, sprite.m_color, {1.0f, 1.0f} };
            vertices[5] = { {-origin.x/size.x, (size.y-origin.y)/size.y, 0.0f}, {worldPos.x, worldPos.y, size.x, size.y}, sprite.m_color, {0.0f, 1.0f} };

            // Create transient vertex buffer
            if (bgfx::getAvailTransientVertexBuffer(6, m_vertexLayout) >= 6) {
                bgfx::TransientVertexBuffer tvb;
                bgfx::allocTransientVertexBuffer(&tvb, 6, m_vertexLayout);
                std::memcpy(tvb.data, vertices, sizeof(vertices));

                // Set vertex buffer
                bgfx::setVertexBuffer(0, &tvb);
                
                // Set render state
                bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
                
                // Submit draw call
                bgfx::submit(info.m_target, m_program);
            }
        });
    }

    return Error{};
}
