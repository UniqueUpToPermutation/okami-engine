#pragma once

#include <filesystem>

namespace okami {
	std::filesystem::path GetExecutablePath();

	std::filesystem::path GetAssetsPath();
	std::filesystem::path GetAssetPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetD3D12ShadersPath();
	std::filesystem::path GetD3D12ShaderPath(const std::filesystem::path& relativePath);

    std::filesystem::path GetBGFXShadersPath();
	std::filesystem::path GetBGFXShaderPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetTestAssetsPath();
	std::filesystem::path GetTestAssetPath(const std::filesystem::path& relativePath);
}