#include "io.hpp"
#include "texture.hpp"
#include "geometry.hpp"
#include "paths.hpp"

#include <glog/logging.h>

namespace okami {
    class TextureIOModule : public IOModule<Texture> {
    protected:
        OnResourceLoadedEvent<Texture> LoadResource(LoadResourceSignal<Texture>&& msg) override {
            auto ext = msg.m_path.extension().string();
            if (ext == ".ktx2" || ext == ".KTX2") {
                return { Texture::FromKTX2(GetAssetPath(msg.m_path), msg.m_params), msg.m_id };
            } else if (ext == ".png" || ext == ".PNG") {
                return { Texture::FromPNG(GetAssetPath(msg.m_path), msg.m_params), msg.m_id };
            } else {
                LOG(ERROR) << "Unsupported texture format for file: " << msg.m_path;
                return { OKAMI_UNEXPECTED("Unsupported texture format: " + ext), msg.m_id };
            }
        }
    };

    class GeometryIOModule : public IOModule<Geometry> {
    protected:
        OnResourceLoadedEvent<Geometry> LoadResource(LoadResourceSignal<Geometry>&& msg) override {
            auto ext = msg.m_path.extension().string();
            if (ext == ".glb" || ext == ".GLB" || ext == ".gltf" || ext == ".GLTF") {
                return { Geometry::LoadGLTF(GetAssetPath(msg.m_path)), msg.m_id };
            } else {
                LOG(ERROR) << "Unsupported geometry format for file: " << msg.m_path;
                return { OKAMI_UNEXPECTED("Unsupported geometry format: " + ext), msg.m_id };
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