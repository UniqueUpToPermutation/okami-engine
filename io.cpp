#include "io.hpp"
#include "texture.hpp"
#include "paths.hpp"

namespace okami {
    class TextureIOModule : public IOModule<Texture> {
    protected:
        OnResourceLoadedMessage<Texture> LoadResource(LoadResourceMessage<Texture>&& msg) override {
            return OnResourceLoadedMessage<Texture>{
                Texture::FromPNG(GetAssetPath(msg.m_path), msg.m_params), 
                msg.m_handle 
            };
        }
    };

    std::unique_ptr<EngineModule> TextureIOModuleFactory::operator()() {
        return std::make_unique<TextureIOModule>();
    }
}