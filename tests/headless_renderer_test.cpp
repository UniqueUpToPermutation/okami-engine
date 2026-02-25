#include <gtest/gtest.h>
#include <filesystem>
#include "../engine.hpp"
#include "../renderer.hpp"
#include "../camera.hpp"
#include "../transform.hpp"
#include "../texture.hpp"
#include "../paths.hpp"

using namespace okami;

class HeadlessRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    
    void TearDown() override {
    }
    
    // Helper function to compare rendered output with golden image
    void CompareWithGoldenImage(const std::string& renderedImagePath, const std::string& goldenImageName) {
        // Load the rendered image
        std::filesystem::path renderedPath(renderedImagePath);
        ASSERT_TRUE(std::filesystem::exists(renderedPath)) 
            << "Rendered image does not exist: " << renderedImagePath;
        
        auto renderedResult = Texture::FromPNG(renderedPath);
        ASSERT_TRUE(renderedResult.has_value()) 
            << "Failed to load rendered image: " << renderedResult.error().Str();
        
        // Load the golden image
        std::filesystem::path goldenPath = GetGoldenImagePath(goldenImageName);
        ASSERT_TRUE(std::filesystem::exists(goldenPath)) 
            << "Golden image does not exist: " << goldenPath;
        
        auto goldenResult = Texture::FromPNG(goldenPath);
        ASSERT_TRUE(goldenResult.has_value()) 
            << "Failed to load golden image: " << goldenResult.error().Str();
        
        const auto& rendered = renderedResult.value();
        const auto& golden = goldenResult.value();
        
        // Compare image dimensions
        ASSERT_EQ(rendered.GetDesc().width, golden.GetDesc().width) 
            << "Image widths don't match";
        ASSERT_EQ(rendered.GetDesc().height, golden.GetDesc().height) 
            << "Image heights don't match";
        ASSERT_EQ(rendered.GetDesc().format, golden.GetDesc().format) 
            << "Image formats don't match";
        
        // Compare pixel data
        auto renderedData = rendered.GetData();
        auto goldenData = golden.GetData();
        
        ASSERT_EQ(renderedData.size(), goldenData.size()) 
            << "Image data sizes don't match";
        
        // Allow for small differences due to floating point precision
        const uint8_t tolerance = 2;
        size_t differentPixels = 0;
        
        for (size_t i = 0; i < renderedData.size(); ++i) {
            if (std::abs(static_cast<int>(renderedData[i]) - static_cast<int>(goldenData[i])) > tolerance) {
                differentPixels++;
            }
        }
        
        // Allow up to 1% of pixels to be different (to account for minor rendering differences)
        size_t totalPixels = renderedData.size() / 4; // Assuming RGBA format
        size_t maxAllowedDifferences = totalPixels / 100; // 1%
        
        EXPECT_LE(differentPixels, maxAllowedDifferences) 
            << "Too many different pixels: " << differentPixels 
            << " out of " << totalPixels << " (max allowed: " << maxAllowedDifferences << ")";
    }
};
