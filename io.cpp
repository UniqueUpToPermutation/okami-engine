#include "io.hpp"
#include "texture.hpp"
#include "geometry.hpp"
#include "paths.hpp"

#include <glog/logging.h>

namespace okami {
    class TextureIOModule : public IOModule<Texture> {
    protected:
        OnResourceLoadedEvent<Texture> LoadResource(LoadResourceSignal<Texture>&& msg) override {
            // Get the extension and decide loading method
            auto ext = msg.m_path.extension().string();
            if (ext == ".ktx2" || ext == ".KTX2") {
                return OnResourceLoadedEvent<Texture>{
                    Texture::FromKTX2(GetAssetPath(msg.m_path), msg.m_params),
                    msg.m_handle
                };
            } else if (ext == ".png" || ext == ".PNG") {
                return OnResourceLoadedEvent<Texture>{
                    Texture::FromPNG(GetAssetPath(msg.m_path), msg.m_params),
                    msg.m_handle
                };
            } else {
                // Unsupported format
                LOG(ERROR) << "Unsupported texture format for file: " << msg.m_path;

                return OnResourceLoadedEvent<Texture>{
                    OKAMI_UNEXPECTED("Unsupported texture format: " + ext),
                    msg.m_handle
                };
            }
        }
    };

    class GeometryIOModule : public IOModule<Geometry> {
    protected:
        OnResourceLoadedEvent<Geometry> LoadResource(LoadResourceSignal<Geometry>&& msg) override {
            // Get the extension and decide loading method
            auto ext = msg.m_path.extension().string();
            if (ext == ".glb" || ext == ".GLB" || ext == ".gltf" || ext == ".GLTF") {
                return OnResourceLoadedEvent<Geometry>{
                    Geometry::LoadGLTF(GetAssetPath(msg.m_path)),
                    msg.m_handle
                };
            } else {
                // Unsupported format
                LOG(ERROR) << "Unsupported geometry format for file: " << msg.m_path;

                return OnResourceLoadedEvent<Geometry>{
                    OKAMI_UNEXPECTED("Unsupported geometry format: " + ext),
                    msg.m_handle
                };
            }
        }
    };

    std::unique_ptr<EngineModule> TextureIOModuleFactory::operator()() {
        return std::make_unique<TextureIOModule>();
    }

    std::unique_ptr<EngineModule> GeometryIOModuleFactory::operator()() {
        return std::make_unique<GeometryIOModule>();
    }
}