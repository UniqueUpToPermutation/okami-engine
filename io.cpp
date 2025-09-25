#include "io.hpp"
#include "texture.hpp"
#include "paths.hpp"

#include <glog/logging.h>

namespace okami {
    class TextureIOModule : public IOModule<Texture> {
    protected:
        OnResourceLoadedMessage<Texture> LoadResource(LoadResourceMessage<Texture>&& msg) override {
            // Get the extension and decide loading method
            auto ext = msg.m_path.extension().string();
            if (ext == ".ktx2" || ext == ".KTX2") {
                return OnResourceLoadedMessage<Texture>{
                    Texture::FromKTX2(GetAssetPath(msg.m_path), msg.m_params),
                    msg.m_handle
                };
            } else if (ext == ".png" || ext == ".PNG") {
                return OnResourceLoadedMessage<Texture>{
                    Texture::FromPNG(GetAssetPath(msg.m_path), msg.m_params),
                    msg.m_handle
                };
            } else {
                // Unsupported format
                LOG(ERROR) << "Unsupported texture format for file: " << msg.m_path;

                return OnResourceLoadedMessage<Texture>{
                    std::unexpected(Error("Unsupported texture format: " + ext)),
                    msg.m_handle
                };
            }
        }
    };

    std::unique_ptr<EngineModule> TextureIOModuleFactory::operator()() {
        return std::make_unique<TextureIOModule>();
    }
}