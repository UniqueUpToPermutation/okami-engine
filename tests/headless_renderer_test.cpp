#include <gtest/gtest.h>
#include <filesystem>
#include "../engine.hpp"
#include "../renderer.hpp"
#include "../camera.hpp"
#include "../transform.hpp"
#include "../webgpu/webgpu_renderer.hpp"
#include "../storage.hpp"
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

// WebGPU Headless Tests
TEST_F(HeadlessRendererTest, CreateEngineWithWebGPURenderer) {
    // Create engine
    Engine engine;

    RendererParams params;
    params.m_headlessRenderOutputDir = "renders";
    params.m_headlessOutputFileStem = "create_engine_with_webgpu_renderer";
    
    // Add modules using factory methods
    engine.CreateRenderModule<WebgpuRendererFactory>(WebgpuRendererFactory{}, params);

    // Initialize the engine
    auto initResult = engine.Startup();
    ASSERT_TRUE(initResult.IsOk()) << "Failed to startup engine: " << initResult;
    
    auto* rendererInterface = engine.QueryInterface<IRenderer>();
    ASSERT_NE(rendererInterface, nullptr) << "Failed to query IRenderer interface";

    // Create a camera entity
    entity_t cameraEntity = engine.CreateEntity();
    engine.AddComponent(cameraEntity, Camera::Identity());
    engine.AddComponent(cameraEntity, Transform::Identity());
    
    // Set the active camera
    engine.SetActiveCamera(cameraEntity);
    EXPECT_EQ(rendererInterface->GetActiveCamera(), cameraEntity);
    
    // Create a triangle entity
    entity_t triangleEntity = engine.CreateEntity();
    engine.AddComponent(triangleEntity, DummyTriangleComponent{});
    engine.AddComponent(triangleEntity, Transform::Identity());

    engine.Run(1); // Run 1 frame for testing
    
    // Shutdown the engine
    engine.Shutdown();
    
    // Compare the rendered output with the golden image
    CompareWithGoldenImage("renders/create_engine_with_webgpu_renderer.png", 
                          "create_engine_with_webgpu_renderer.png");
}

TEST_F(HeadlessRendererTest, WebGPUTriangleRenderingInHeadlessMode) {
    // Create engine
    Engine engine;

    RendererParams params;
    params.m_headlessRenderOutputDir = "renders";
    params.m_headlessOutputFileStem = "webgpu_triangle_rendering_in_headless_mode";
    
    // Add modules using factory methods
    engine.CreateRenderModule<WebgpuRendererFactory>(WebgpuRendererFactory{}, params);

    // Initialize the engine
    auto initResult = engine.Startup();
    ASSERT_TRUE(initResult.IsOk()) << "Failed to startup engine: " << initResult;
    
    auto* rendererInterface = engine.QueryInterface<IRenderer>();
    ASSERT_NE(rendererInterface, nullptr) << "Failed to query IRenderer interface";

    // Create a camera entity
    entity_t cameraEntity = engine.CreateEntity();
    engine.AddComponent(cameraEntity, Camera::Identity());
    engine.AddComponent(cameraEntity, Transform::Identity());
    
    // Set the active camera
    engine.SetActiveCamera(cameraEntity);
    EXPECT_EQ(rendererInterface->GetActiveCamera(), cameraEntity);
    
    // Create a triangle entity
    entity_t triangleEntity = engine.CreateEntity();
    engine.AddComponent(triangleEntity, DummyTriangleComponent{});
    engine.AddComponent(triangleEntity, Transform::Translate(-0.5f, 0.0f, 0.0f));

    triangleEntity = engine.CreateEntity();
    engine.AddComponent(triangleEntity, DummyTriangleComponent{});
    engine.AddComponent(triangleEntity, Transform::Translate(0.5f, 0.0f, 0.0f));

    engine.Run(1); // Run 1 frame for testing
    
    // Shutdown the engine
    engine.Shutdown();
    
    // Compare the rendered output with the golden image
    CompareWithGoldenImage("renders/webgpu_triangle_rendering_in_headless_mode.png", 
                          "webgpu_triangle_rendering_in_headless_mode.png");
}

TEST_F(HeadlessRendererTest, CreateTexture) {
    // Create engine
    Engine engine;

    RendererParams params;
    params.m_headlessRenderToFile = false;
    
    // Add modules using factory methods
    engine.CreateRenderModule<WebgpuRendererFactory>(WebgpuRendererFactory{}, params);

    // Initialize the engine
    auto initResult = engine.Startup();
    ASSERT_TRUE(initResult.IsOk()) << "Failed to startup engine: " << initResult;

    auto handle = engine.LoadResource<Texture>(GetTestAssetPath("test.ktx2"));

    engine.AddScript([handle](Time const& t, ModuleInterface& mi) {
       if (handle.IsLoaded()) {
           mi.m_messages.Send(SignalExit{});
       }
    });

    engine.Run(1000);

    ASSERT_TRUE(handle.IsLoaded()) << "Texture failed to load: " << handle.GetPath();
    ASSERT_EQ(handle->width, 128);
    ASSERT_EQ(handle->height, 128);
}