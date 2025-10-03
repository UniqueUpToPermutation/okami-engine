// PNG to KTX2 converter with mipmap generation
// Usage: png2ktx input.png output.ktx2 [--quiet]

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cmath>
#include "lodepng.h"
#include <ktx.h>

bool g_quiet = false;
bool g_debug = false;
bool g_linear = false;

// sRGB to linear conversion
inline float srgbToLinear(unsigned char srgb) {
    float s = srgb / 255.0f;
    if (s <= 0.04045f) {
        return s / 12.92f;
    } else {
        return std::pow((s + 0.055f) / 1.055f, 2.4f);
    }
}

// Linear to sRGB conversion
inline unsigned char linearToSrgb(float linear) {
    float s;
    if (linear <= 0.0031308f) {
        s = linear * 12.92f;
    } else {
        s = 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    }
    float result = s * 255.0f + 0.5f;
    result = std::max(0.0f, std::min(255.0f, result));
    return static_cast<unsigned char>(result);
}

// Gamma-correct box filter for downsampling
// Converts to linear space, averages, then converts back to sRGB
// Or uses simple linear blending if g_linear is true
void generateMipmap(const unsigned char* src, unsigned char* dst, 
                    unsigned int srcWidth, unsigned int srcHeight) {
    unsigned int dstWidth = std::max(1u, srcWidth / 2);
    unsigned int dstHeight = std::max(1u, srcHeight / 2);
    
    for (unsigned int y = 0; y < dstHeight; ++y) {
        for (unsigned int x = 0; x < dstWidth; ++x) {
            unsigned int srcX = x * 2;
            unsigned int srcY = y * 2;
            
            if (g_linear) {
                // Simple linear averaging in sRGB space (faster but incorrect)
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
                dst[dstIdx + 0] = r / samples;
                dst[dstIdx + 1] = g / samples;
                dst[dstIdx + 2] = b / samples;
                dst[dstIdx + 3] = a / samples;
            } else {
                // Gamma-correct averaging in linear space (correct)
                float r = 0.0f, g = 0.0f, b = 0.0f;
                unsigned int a = 0;
                int samples = 0;
                
                for (int dy = 0; dy < 2 && (srcY + dy) < srcHeight; ++dy) {
                    for (int dx = 0; dx < 2 && (srcX + dx) < srcWidth; ++dx) {
                        unsigned int idx = ((srcY + dy) * srcWidth + (srcX + dx)) * 4;
                        
                        // Convert RGB to linear space for proper averaging
                        r += srgbToLinear(src[idx + 0]);
                        g += srgbToLinear(src[idx + 1]);
                        b += srgbToLinear(src[idx + 2]);
                        
                        // Alpha is linear, average directly
                        a += src[idx + 3];
                        samples++;
                    }
                }
                
                // Average in linear space
                r /= samples;
                g /= samples;
                b /= samples;
                a /= samples;
                
                // Convert back to sRGB
                unsigned int dstIdx = (y * dstWidth + x) * 4;
                dst[dstIdx + 0] = linearToSrgb(r);
                dst[dstIdx + 1] = linearToSrgb(g);
                dst[dstIdx + 2] = linearToSrgb(b);
                dst[dstIdx + 3] = static_cast<unsigned char>(a);
            }
        }
    }
}

unsigned int calculateMipLevels(unsigned int width, unsigned int height) {
    unsigned int levels = 1;
    unsigned int size = std::max(width, height);
    while (size > 1) {
        size /= 2;
        levels++;
    }
    return levels;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " input.png output.ktx2 [--quiet] [--debug] [--linear]" << std::endl;
        return 1;
    }
    
    const char* inputFile = argv[1];
    const char* outputFile = argv[2];
    
    // Parse all flags
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--quiet") == 0) {
            g_quiet = true;
        } else if (std::strcmp(argv[i], "--debug") == 0) {
            g_debug = true;
        } else if (std::strcmp(argv[i], "--linear") == 0) {
            g_linear = true;
        } else {
            std::cerr << "Unknown flag: " << argv[i] << std::endl;
            std::cerr << "Usage: " << argv[0] << " input.png output.ktx2 [--quiet] [--debug] [--linear]" << std::endl;
            return 1;
        }
    }
    
    if (g_linear && !g_quiet) {
        std::cout << "Using linear blending (no gamma correction)" << std::endl;
    }
    
    // Load PNG file
    if (!g_quiet) std::cout << "Loading PNG: " << inputFile << std::endl;
    unsigned char* imageData = nullptr;
    unsigned int width = 0, height = 0;
    unsigned int error = lodepng_decode32_file(&imageData, &width, &height, inputFile);
    
    if (error) {
        std::cerr << "Error loading PNG: " << lodepng_error_text(error) << std::endl;
        return 1;
    }
    
    if (!g_quiet) std::cout << "Image loaded: " << width << "x" << height << std::endl;
    
    // Calculate number of mip levels
    unsigned int numLevels = calculateMipLevels(width, height);
    if (!g_quiet) std::cout << "Generating " << numLevels << " mip levels..." << std::endl;
    
    // Create KTX2 texture
    ktxTextureCreateInfo createInfo = {};
    createInfo.glInternalformat = 0; // For KTX2, use VkFormat instead
    createInfo.vkFormat = 37; // VK_FORMAT_R8G8B8A8_UNORM (RGBA8)
    createInfo.baseWidth = width;
    createInfo.baseHeight = height;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = numLevels;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE; // We'll generate them manually
    
    ktxTexture2* texture = nullptr;
    KTX_error_code result = ktxTexture2_Create(&createInfo, 
                                               KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                               &texture);
    
    if (result != KTX_SUCCESS) {
        std::cerr << "Error creating KTX2 texture: " << ktxErrorString(result) << std::endl;
        free(imageData);
        return 1;
    }
    
    // Store mipmap data
    std::vector<std::vector<unsigned char>> mipmaps(numLevels);
    mipmaps[0].assign(imageData, imageData + (width * height * 4));
    
    // Generate mipmaps
    unsigned int mipWidth = width;
    unsigned int mipHeight = height;
    
    for (unsigned int level = 0; level < numLevels; ++level) {
        if (!g_quiet) std::cout << "  Level " << level << ": " << mipWidth << "x" << mipHeight << std::endl;
        
        // Set image data for this level
        const unsigned char* srcData = (level == 0) ? imageData : mipmaps[level].data();
        result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                              level,
                                              0, // layer
                                              0, // faceSlice
                                              srcData,
                                              mipWidth * mipHeight * 4);
        
        if (result != KTX_SUCCESS) {
            std::cerr << "Error setting image data for level " << level << ": " 
                      << ktxErrorString(result) << std::endl;
            ktxTexture_Destroy(ktxTexture(texture));
            free(imageData);
            return 1;
        }
        
        // Save mipmap level to PNG in debug mode
        if (g_debug) {
            // Generate debug filename: output_mip0.png, output_mip1.png, etc.
            std::string debugFilename = outputFile;
            size_t dotPos = debugFilename.rfind('.');
            if (dotPos != std::string::npos) {
                debugFilename = debugFilename.substr(0, dotPos);
            }
            debugFilename += "_mip" + std::to_string(level) + ".png";
            
            unsigned int encodeError = lodepng_encode32_file(
                debugFilename.c_str(),
                srcData,
                mipWidth,
                mipHeight
            );
            
            if (encodeError) {
                std::cerr << "Warning: Failed to save debug mipmap " << level << ": " 
                         << lodepng_error_text(encodeError) << std::endl;
            } else {
                std::cout << "  Saved debug mipmap: " << debugFilename << std::endl;
            }
        }
        
        // Generate next mip level if not the last
        if (level < numLevels - 1) {
            unsigned int nextWidth = std::max(1u, mipWidth / 2);
            unsigned int nextHeight = std::max(1u, mipHeight / 2);
            mipmaps[level + 1].resize(nextWidth * nextHeight * 4);
            
            generateMipmap(srcData, mipmaps[level + 1].data(), mipWidth, mipHeight);
            
            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }
    }
    
    // Write to file
    if (!g_quiet) std::cout << "Writing KTX2 file: " << outputFile << std::endl;
    result = ktxTexture_WriteToNamedFile(ktxTexture(texture), outputFile);
    
    if (result != KTX_SUCCESS) {
        std::cerr << "Error writing KTX2 file: " << ktxErrorString(result) << std::endl;
        ktxTexture_Destroy(ktxTexture(texture));
        free(imageData);
        return 1;
    }
    
    if (!g_quiet) std::cout << "Successfully created " << outputFile << std::endl;
    
    // Cleanup
    ktxTexture_Destroy(ktxTexture(texture));
    free(imageData);
    
    return 0;
}
