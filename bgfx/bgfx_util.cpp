#include "bgfx_util.hpp"

#include "paths.hpp"

#include <fstream>
#include <vector>

#include <glog/logging.h>

using namespace okami;

static std::filesystem::path GetShaderPath(std::filesystem::path path) {
    path = GetBGFXShaderPath(path);

    path.replace_extension("");

    switch (bgfx::getRendererType() )
	{
	case bgfx::RendererType::Noop:
	case bgfx::RendererType::Direct3D11:
	case bgfx::RendererType::Direct3D12: path += ("_dx11");  break;
	case bgfx::RendererType::Agc:
	case bgfx::RendererType::Gnm:        path += ("_pssl");  break;
	case bgfx::RendererType::Metal:      path += ("_metal"); break;
	case bgfx::RendererType::Nvn:        path += ("_nvn");   break;
	case bgfx::RendererType::OpenGL:     path += ("_opengl");  break;
	case bgfx::RendererType::OpenGLES:   path += ("_essl");  break;
	case bgfx::RendererType::Vulkan:     path += ("_spirv"); break;

	case bgfx::RendererType::Count:
		BX_ASSERT(false, "You should not be here!");
		break;
	}

    path += (".bin");
    return path;
}

Expected<AutoHandle<bgfx::ShaderHandle>> okami::LoadBgfxShader(
    std::filesystem::path path) {

    path = GetShaderPath(path);
    if (!std::filesystem::exists(path)) {
        return std::unexpected(Error("Shader file does not exist: " + path.string()));
    }

    LOG(INFO) << "Loading shader: " << path;

    // Load the .bin file and create the shader
    try {
        // Open the file and read its contents
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return std::unexpected(Error("Failed to open shader file: " + path.string()));
        }

        // Get file size and read contents
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return std::unexpected(Error("Failed to read shader file: " + path.string()));
        }

        // Create bgfx memory reference and shader handle
        const bgfx::Memory* mem = bgfx::copy(buffer.data(), static_cast<uint32_t>(buffer.size()));
        bgfx::ShaderHandle handle = bgfx::createShader(mem);
        
        if (!bgfx::isValid(handle)) {
            return std::unexpected(Error("Failed to create bgfx shader from file: " + path.string()));
        }

        return AutoHandle<bgfx::ShaderHandle>(handle);
    } catch (const std::exception& e) {
        return std::unexpected(Error("Exception while loading shader file " + path.string() + ": " + e.what()));
    }
}