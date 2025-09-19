#include "bgfx_util.hpp"

using namespace okami;

std::optional<AutoHandle<bgfx::ShaderHandle>> okami::LoadBgfxShader(
    std::filesystem::path path) {
    
    /*switch (bgfx::getRendererType() )
	{
	case bgfx::RendererType::Noop:
	case bgfx::RendererType::Direct3D11:
	case bgfx::RendererType::Direct3D12: path.append("dx11");  break;
	case bgfx::RendererType::Agc:
	case bgfx::RendererType::Gnm:        path.append("pssl");  break;
	case bgfx::RendererType::Metal:      path.append("metal"); break;
	case bgfx::RendererType::Nvn:        path.append("nvn");   break;
	case bgfx::RendererType::OpenGL:     path.append("glsl");  break;
	case bgfx::RendererType::OpenGLES:   path.append("essl");  break;
	case bgfx::RendererType::Vulkan:     path.append("spirv"); break;

	case bgfx::RendererType::Count:
		BX_ASSERT(false, "You should not be here!");
		break;
	}

	char fileName[512];
	bx::strCopy(fileName, BX_COUNTOF(fileName), _name);
	bx::strCat(fileName, BX_COUNTOF(fileName), ".bin");

	filePath.join(fileName);

	bgfx::ShaderHandle handle = bgfx::createShader(loadMem(_reader, filePath.getCPtr() ) );
	bgfx::setName(handle, _name.getPtr(), _name.getLength() );*/

    return {};
}