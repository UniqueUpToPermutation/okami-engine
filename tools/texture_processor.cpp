#include "texture_processor.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <ktx.h>

// ---------------------------------------------------------------------------
// Internal helpers (not exposed in header)
// ---------------------------------------------------------------------------

static float SrgbToLinear(unsigned char srgb) {
    float s = srgb / 255.0f;
    if (s <= 0.04045f)
        return s / 12.92f;
    return std::pow((s + 0.055f) / 1.055f, 2.4f);
}

static unsigned char LinearToSrgb(float linear) {
    float s;
    if (linear <= 0.0031308f)
        s = linear * 12.92f;
    else
        s = 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    float result = s * 255.0f + 0.5f;
    result = std::max(0.0f, std::min(255.0f, result));
    return static_cast<unsigned char>(result);
}

static void GenerateMipmap(const unsigned char* src, unsigned char* dst,
                            unsigned int srcWidth, unsigned int srcHeight,
                            bool linearMips) {
    unsigned int dstWidth  = std::max(1u, srcWidth  / 2);
    unsigned int dstHeight = std::max(1u, srcHeight / 2);

    for (unsigned int y = 0; y < dstHeight; ++y) {
        for (unsigned int x = 0; x < dstWidth; ++x) {
            unsigned int srcX = x * 2;
            unsigned int srcY = y * 2;

            if (linearMips) {
                unsigned int r = 0, g = 0, b = 0, a = 0;
                int samples = 0;
                for (int dy = 0; dy < 2 && (srcY + dy) < srcHeight; ++dy) {
                    for (int dx = 0; dx < 2 && (srcX + dx) < srcWidth; ++dx) {
                        unsigned int idx = ((srcY + dy) * srcWidth + (srcX + dx)) * 4;
                        r += src[idx + 0];
                        g += src[idx + 1];
                        b += src[idx + 2];
                        a += src[idx + 3];
                        samples++;
                    }
                }
                unsigned int dstIdx = (y * dstWidth + x) * 4;
                dst[dstIdx + 0] = static_cast<unsigned char>(r / samples);
                dst[dstIdx + 1] = static_cast<unsigned char>(g / samples);
                dst[dstIdx + 2] = static_cast<unsigned char>(b / samples);
                dst[dstIdx + 3] = static_cast<unsigned char>(a / samples);
            } else {
                float r = 0.0f, g = 0.0f, b = 0.0f;
                unsigned int a = 0;
                int samples = 0;
                for (int dy = 0; dy < 2 && (srcY + dy) < srcHeight; ++dy) {
                    for (int dx = 0; dx < 2 && (srcX + dx) < srcWidth; ++dx) {
                        unsigned int idx = ((srcY + dy) * srcWidth + (srcX + dx)) * 4;
                        r += SrgbToLinear(src[idx + 0]);
                        g += SrgbToLinear(src[idx + 1]);
                        b += SrgbToLinear(src[idx + 2]);
                        a += src[idx + 3];
                        samples++;
                    }
                }
                r /= samples;
                g /= samples;
                b /= samples;
                a /= samples;
                unsigned int dstIdx = (y * dstWidth + x) * 4;
                dst[dstIdx + 0] = LinearToSrgb(r);
                dst[dstIdx + 1] = LinearToSrgb(g);
                dst[dstIdx + 2] = LinearToSrgb(b);
                dst[dstIdx + 3] = static_cast<unsigned char>(a);
            }
        }
    }
}

static unsigned int CalculateMipLevels(unsigned int width, unsigned int height) {
    unsigned int levels = 1;
    unsigned int size = std::max(width, height);
    while (size > 1) {
        size /= 2;
        levels++;
    }
    return levels;
}

// ---------------------------------------------------------------------------
// TextureProcessor
// ---------------------------------------------------------------------------

TextureProcessor::TextureProcessor(bool quiet, TextureProcessorParams initialParams)
    : m_quiet(quiet), m_stack({initialParams}) {}

static std::string LowerExt(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

bool TextureProcessor::CanProcess(const std::filesystem::path& inputPath) const {
    auto ext = LowerExt(inputPath);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

std::filesystem::path TextureProcessor::OutputPath(
        const std::filesystem::path& inputRelPath) const {
    return std::filesystem::path(inputRelPath).replace_extension(".ktx2");
}

void TextureProcessor::PushSettings(const YAML::Node& settings) {
    // Start from the current top so all existing values are inherited.
    TextureProcessorParams top = m_stack.back();
    if (settings && settings.IsMap()) {
        if (settings["build_mips"])
            top.buildMips   = settings["build_mips"].as<bool>();
        if (settings["linear_mips"])
            top.linearMips  = settings["linear_mips"].as<bool>();
        if (settings["copy_source"])
            top.copySource  = settings["copy_source"].as<bool>();
    }
    m_stack.push_back(top);
}

void TextureProcessor::PopSettings() {
    if (m_stack.size() > 1)
        m_stack.pop_back();
}

void TextureProcessor::SetVirtualOverride(const std::filesystem::path& absInputPath,
                                          const YAML::Node& overrides) {
    m_overrides[absInputPath] = overrides;
}

TextureProcessorParams TextureProcessor::EffectiveParams(
        const std::filesystem::path& input) const {
    TextureProcessorParams p = m_stack.back();
    auto it = m_overrides.find(input);
    if (it != m_overrides.end()) {
        const YAML::Node& ovr = it->second;
        if (ovr["build_mips"])  p.buildMips  = ovr["build_mips"].as<bool>();
        if (ovr["linear_mips"]) p.linearMips = ovr["linear_mips"].as<bool>();
        if (ovr["copy_source"]) p.copySource = ovr["copy_source"].as<bool>();
    }
    return p;
}

std::filesystem::path TextureProcessor::SidecarPath(
        const std::filesystem::path& output) {
    return std::filesystem::path(output.string() + ".settings");
}

void TextureProcessor::WriteSidecar(const std::filesystem::path& output,
                                     const TextureProcessorParams& params) const {
    YAML::Node n;
    n["build_mips"]  = params.buildMips;
    n["linear_mips"] = params.linearMips;
    n["copy_source"] = params.copySource;
    std::ofstream f(SidecarPath(output));
    if (f.is_open())
        f << n;
}

bool TextureProcessor::NeedsProcessing(const std::filesystem::path& input,
                                        const std::filesystem::path& output) const {
    if (!std::filesystem::exists(output))
        return true;
    if (std::filesystem::last_write_time(input) >
        std::filesystem::last_write_time(output))
        return true;

    // Compare stored settings (sidecar) against current effective settings.
    std::filesystem::path sidecar = SidecarPath(output);
    if (!std::filesystem::exists(sidecar))
        return true; // first build with new sidecar feature

    try {
        auto stored  = YAML::LoadFile(sidecar.string());
        auto current = EffectiveParams(input);
        if (stored["build_mips"].as<bool>(true)   != current.buildMips)  return true;
        if (stored["linear_mips"].as<bool>(false)  != current.linearMips) return true;
        if (stored["copy_source"].as<bool>(false)  != current.copySource) return true;
    } catch (...) {
        return true; // unreadable sidecar — rebuild to be safe
    }
    return false;
}

void TextureProcessor::Process(const std::filesystem::path& input,
                                 const std::filesystem::path& output) {
    if (!m_quiet)
        std::cout << "texture: " << input.filename().string()
                  << " -> " << output.filename().string() << std::endl;

    // Compute effective settings once: YAML stack + any virtual override for this file.
    auto params = EffectiveParams(input);

    int imgWidth = 0, imgHeight = 0, imgChannels = 0;
    unsigned char* imageData = stbi_load(input.string().c_str(),
                                         &imgWidth, &imgHeight, &imgChannels, 4);
    if (!imageData)
        throw std::runtime_error("Failed to load image '" + input.string()
                                 + "': " + stbi_failure_reason());

    unsigned int width  = static_cast<unsigned int>(imgWidth);
    unsigned int height = static_cast<unsigned int>(imgHeight);
    unsigned int numLevels = params.buildMips ? CalculateMipLevels(width, height) : 1;

    if (!m_quiet)
        std::cout << "  " << width << "x" << height
                  << ", " << numLevels << " mip level(s)" << std::endl;

    ktxTextureCreateInfo createInfo = {};
    createInfo.vkFormat      = 37; // VK_FORMAT_R8G8B8A8_UNORM
    createInfo.baseWidth     = width;
    createInfo.baseHeight    = height;
    createInfo.baseDepth     = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels     = numLevels;
    createInfo.numLayers     = 1;
    createInfo.numFaces      = 1;
    createInfo.isArray       = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;

    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_Create(&createInfo,
                                               KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                               &texture);
    if (result != KTX_SUCCESS) {
        stbi_image_free(imageData);
        throw std::runtime_error("ktxTexture2_Create failed: "
                                 + std::string(ktxErrorString(result)));
    }

    std::vector<std::vector<unsigned char>> mipmaps(numLevels);
    mipmaps[0].assign(imageData, imageData + (width * height * 4));

    unsigned int mipWidth  = width;
    unsigned int mipHeight = height;

    for (unsigned int level = 0; level < numLevels; ++level) {
        const unsigned char* srcData = (level == 0) ? imageData
                                                    : mipmaps[level].data();

        result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                               level, 0, 0,
                                               srcData,
                                               mipWidth * mipHeight * 4);
        if (result != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTexture(texture));
            stbi_image_free(imageData);
            throw std::runtime_error("ktxTexture_SetImageFromMemory failed at level "
                                     + std::to_string(level) + ": "
                                     + std::string(ktxErrorString(result)));
        }

        if (level < numLevels - 1) {
            unsigned int nextWidth  = std::max(1u, mipWidth  / 2);
            unsigned int nextHeight = std::max(1u, mipHeight / 2);
            mipmaps[level + 1].resize(nextWidth * nextHeight * 4);
            GenerateMipmap(srcData, mipmaps[level + 1].data(),
                           mipWidth, mipHeight, params.linearMips);
            mipWidth  = nextWidth;
            mipHeight = nextHeight;
        }
    }

    result = ktxTexture_WriteToNamedFile(ktxTexture(texture),
                                         output.string().c_str());
    ktxTexture_Destroy(ktxTexture(texture));
    stbi_image_free(imageData);

    if (result != KTX_SUCCESS)
        throw std::runtime_error("Failed to write KTX2 file '" + output.string()
                                 + "': " + std::string(ktxErrorString(result)));

    if (params.copySource) {
        std::filesystem::path sourceCopy = output.parent_path() / input.filename();
        std::filesystem::copy_file(input, sourceCopy,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    // Record the settings used so future runs can detect changes via NeedsProcessing.
    WriteSidecar(output, params);
}
