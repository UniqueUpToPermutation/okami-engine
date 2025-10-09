#pragma once

#include <filesystem>

namespace okami {
	struct PathHash {
        std::size_t operator()(const std::filesystem::path& p) const {
            // Use lexically_normal() instead of canonical() to avoid exceptions
            // when the path doesn't exist. This still normalizes the path
            // (resolves . and .. components) but doesn't require file system access.
            return std::hash<std::string>{}(p.lexically_normal().string());
        }
    };

	std::filesystem::path GetExecutablePath();
	std::filesystem::path GetExecutableRelativePath(const std::filesystem::path& relativePath);

	std::filesystem::path GetAssetsPath();
	std::filesystem::path GetAssetPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetD3D12ShadersPath();
	std::filesystem::path GetD3D12ShaderPath(const std::filesystem::path& relativePath);

    std::filesystem::path GetBGFXShadersPath();
	std::filesystem::path GetBGFXShaderPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetWebGPUShadersPath();
	std::filesystem::path GetWebGPUShaderPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetGLSLShadersPath();
	std::filesystem::path GetGLSLShaderPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetTestAssetsPath();
	std::filesystem::path GetTestAssetPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetConfigsPath();
	std::filesystem::path GetConfigPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetGoldenImagesPath();
	std::filesystem::path GetGoldenImagePath(const std::filesystem::path& relativePath);
}