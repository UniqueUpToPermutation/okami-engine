#include "paths.hpp"
#include <optional>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>
#include <climits>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <vector>
#endif

using namespace okami;

std::optional<std::filesystem::path> g_exePath = std::nullopt;
std::optional<std::filesystem::path> g_assetsPath = std::nullopt;
std::optional<std::filesystem::path> g_d3d12ShadersPath = std::nullopt;
std::optional<std::filesystem::path> g_bgfxShadersPath = std::nullopt;
std::optional<std::filesystem::path> g_webgpuShadersPath = std::nullopt;
std::optional<std::filesystem::path> g_testAssetsPath = std::nullopt;
std::optional<std::filesystem::path> g_configsPath = std::nullopt;

static std::filesystem::path SearchForPath(const std::filesystem::path& startPath, const std::string& targetDir) {
    // Check current directory for "assets"
    std::filesystem::path currentDir = GetExecutablePath().parent_path();
    std::filesystem::path assetsDir = currentDir / targetDir;
    if (std::filesystem::exists(assetsDir) && std::filesystem::is_directory(assetsDir)) {
        return assetsDir;
    }

    // Go up the directory tree to find "assets"
    while (currentDir.has_parent_path() && currentDir != currentDir.root_path()) {
        currentDir = currentDir.parent_path();
        assetsDir = currentDir / targetDir;
        if (std::filesystem::exists(assetsDir) && std::filesystem::is_directory(assetsDir)) {
            return assetsDir;
        }
    }

    // If not found, return empty path
    return std::filesystem::path();
}

static std::filesystem::path FindAssetsPath() {
	return SearchForPath(std::filesystem::current_path(), "assets");
}

static std::filesystem::path FindD3D12ShadersPath() {
    return SearchForPath(std::filesystem::current_path(), "d3d12/shaders");
}

static std::filesystem::path FindBGFXShadersPath() {
    return SearchForPath(std::filesystem::current_path(), "bgfx/shaders");
}

static std::filesystem::path FindWebGPUShadersPath() {
    return SearchForPath(std::filesystem::current_path(), "webgpu/shaders");
}

static std::filesystem::path FindTestAssetsPath() {
    return SearchForPath(std::filesystem::current_path(), "tests/assets");
}

static std::filesystem::path FindConfigsPath() {
    return SearchForPath(std::filesystem::current_path(), "config");
}

static std::filesystem::path FindExecutablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer);
    }
#elif __linux__
    char buffer[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length != -1) {
        buffer[length] = '\0';
        return std::filesystem::path(buffer);
    }
#elif __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        return std::filesystem::path(buffer.data());
    }
#endif
    return std::filesystem::path();
}

std::filesystem::path okami::GetAssetsPath() {
	if (!g_assetsPath.has_value()) {
		g_assetsPath = FindAssetsPath();
	}
	return *g_assetsPath;
}

std::filesystem::path okami::GetAssetPath(const std::filesystem::path& relativePath) {
    return GetAssetsPath() / relativePath;
}

std::filesystem::path okami::GetD3D12ShadersPath() {
    if (!g_d3d12ShadersPath.has_value()) {
        g_d3d12ShadersPath = FindD3D12ShadersPath();
    }
	return *g_d3d12ShadersPath;
}

std::filesystem::path okami::GetD3D12ShaderPath(const std::filesystem::path& relativePath) {
	return GetD3D12ShadersPath() / relativePath;
}

std::filesystem::path okami::GetBGFXShadersPath() {
	if (!g_bgfxShadersPath.has_value()) {
        g_bgfxShadersPath = FindBGFXShadersPath();
    }
    return *g_bgfxShadersPath;
}

std::filesystem::path okami::GetBGFXShaderPath(const std::filesystem::path& relativePath) {
    return GetBGFXShadersPath() / relativePath;
}

std::filesystem::path okami::GetWebGPUShadersPath() {
	if (!g_webgpuShadersPath.has_value()) {
        g_webgpuShadersPath = FindWebGPUShadersPath();
    }
    return *g_webgpuShadersPath;
}

std::filesystem::path okami::GetWebGPUShaderPath(const std::filesystem::path& relativePath) {
    return GetWebGPUShadersPath() / relativePath;
}

std::filesystem::path okami::GetExecutablePath() {
    if (!g_exePath.has_value()) {
        g_exePath = FindExecutablePath();
    }
    return *g_exePath;
}

std::filesystem::path okami::GetExecutableRelativePath(const std::filesystem::path& relativePath) {
    return GetExecutablePath().parent_path() / relativePath;
}

std::filesystem::path okami::GetTestAssetsPath() {
    if (!g_testAssetsPath.has_value()) {
        g_testAssetsPath = FindTestAssetsPath();
    }
    return *g_testAssetsPath;
}

std::filesystem::path okami::GetTestAssetPath(const std::filesystem::path& relativePath) {
    return GetTestAssetsPath() / relativePath;
}

std::filesystem::path okami::GetConfigsPath() {
    if (!g_configsPath.has_value()) {
        g_configsPath = FindConfigsPath();
    }
    return *g_configsPath;
}

std::filesystem::path okami::GetConfigPath(const std::filesystem::path& relativePath) {
    return GetConfigsPath() / relativePath;
}