#include <gtest/gtest.h>
#include <filesystem>
#include "../engine.hpp"
#include "../renderer.hpp"
#include "../camera.hpp"
#include "../transform.hpp"
#include "../bgfx/bgfx_renderer.hpp"
#include "../storage.hpp"

using namespace okami;

class HeadlessRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test output directory
        std::filesystem::create_directories("test_output");
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all("test_output");
    }
};

TEST_F(HeadlessRendererTest, CreateEngineWithBgfxRenderer) {
    // Create engine
    Engine engine;

    RendererParams params;
    params.m_headlessMode = true; // Enable headless mode for testing
    params.m_headlessRenderOutputDir = "renders";
    params.m_headlessOutputFileStem = "create_engine_with_bgfx_renderer";
    
    // Add modules using factory methods
    engine.CreateRenderModule<BgfxRendererFactory>({}, params);

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
}
