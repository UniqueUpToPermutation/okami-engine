#include <gtest/gtest.h>
#include <filesystem>
#include "../engine.hpp"
#include "../renderer.hpp"
#include "../paths.hpp"
#include "../texture.hpp"
#include "../glfw_module.hpp"
#include "../ogl/ogl_renderer.hpp"
#include "../ogl/ogl_headless.hpp"
#include "../camera_controllers.hpp"

// Sample scene setup functions – each sample exposes SetupScene() in scene.hpp.
#include "../samples/01_hello_world/scene.hpp"
#include "../samples/02_zoo/scene.hpp"
#include "../samples/03_materials/scene.hpp"

using namespace okami;

// ---------------------------------------------------------------------------
// Test fixture helpers
// ---------------------------------------------------------------------------

class HeadlessRendererTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Returns the capture directory for a named test.
    static std::filesystem::path CaptureDir(const char* testName) {
        return GetTestAssetsPath() / "headless_output" / testName;
    }

    // Returns the path of the frame captured at the given index.
    static std::filesystem::path FramePath(const std::filesystem::path& captureDir,
                                            size_t frameIndex) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "frame_%04zu.png", frameIndex);
        return captureDir / buf;
    }

    // Compare a rendered image against the committed golden image.
    // On first run (no golden exists) the rendered image is saved as the new
    // golden and the test is skipped, so you can review and commit it.
    void CompareWithGoldenImage(const std::filesystem::path& renderedPath,
                                const char* goldenName) {
        ASSERT_TRUE(std::filesystem::exists(renderedPath))
            << "Rendered image does not exist: " << renderedPath;

        auto renderedResult = Texture::FromPNG(renderedPath);
        ASSERT_TRUE(renderedResult.has_value())
            << "Failed to load rendered image: " << renderedResult.error().Str();

        std::filesystem::path goldenPath = GetGoldenImagePath(goldenName);

        // First-run / update mode: golden doesn't exist yet – save it.
        if (!std::filesystem::exists(goldenPath)) {
            std::filesystem::create_directories(goldenPath.parent_path());
            std::filesystem::copy_file(renderedPath, goldenPath,
                std::filesystem::copy_options::overwrite_existing);
            GTEST_SKIP() << "Golden image created at " << goldenPath
                         << ". Re-run the test to compare.";
            return;
        }

        auto goldenResult = Texture::FromPNG(goldenPath);
        ASSERT_TRUE(goldenResult.has_value())
            << "Failed to load golden image: " << goldenResult.error().Str();

        const auto& rendered = renderedResult.value();
        const auto& golden   = goldenResult.value();

        ASSERT_EQ(rendered.GetDesc().width,  golden.GetDesc().width)  << "Image widths don't match";
        ASSERT_EQ(rendered.GetDesc().height, golden.GetDesc().height) << "Image heights don't match";
        ASSERT_EQ(rendered.GetDesc().format, golden.GetDesc().format) << "Image formats don't match";

        auto renderedData = rendered.GetData();
        auto goldenData   = golden.GetData();
        ASSERT_EQ(renderedData.size(), goldenData.size()) << "Image data sizes don't match";

        // Allow small per-channel differences from floating-point precision.
        const uint8_t tolerance = 2;
        size_t differentPixels  = 0;
        for (size_t i = 0; i < renderedData.size(); ++i) {
            if (std::abs(static_cast<int>(renderedData[i]) -
                         static_cast<int>(goldenData[i])) > tolerance) {
                ++differentPixels;
            }
        }

        // Up to 1 % of pixels may differ.
        size_t totalPixels           = renderedData.size() / 4; // RGBA
        size_t maxAllowedDifferences = totalPixels / 100;
        EXPECT_LE(differentPixels, maxAllowedDifferences)
            << "Too many different pixels: " << differentPixels
            << " out of " << totalPixels
            << " (max allowed: " << maxAllowedDifferences << ")";
    }

    // Number of frames rendered per test.  15 frames gives async texture /
    // geometry loads enough ticks to upload to the GPU.
    static constexpr size_t kFrameCount = 15;
};

// ---------------------------------------------------------------------------
// Sample 01 – Hello World
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, HelloWorld) {
    auto captureDir = CaptureDir("hello_world");

    Engine en;
    en.CreateModule<HeadlessGLFWModuleFactory>({}, HeadlessGLParams{
        .m_size       = {800, 600},
        .m_captureDir = captureDir,
    });
    en.CreateModule<OGLRendererFactory>({}, RendererParams{});

    ASSERT_FALSE(en.Startup().IsError());

    // Use the exact same scene setup as the sample (camera rotation script
    // is intentionally omitted to keep output deterministic).
    sample_hello_world::SetupScene(en);

    en.Run(kFrameCount);
    en.Shutdown();

    CompareWithGoldenImage(FramePath(captureDir, kFrameCount - 1), "hello_world.png");
}

// ---------------------------------------------------------------------------
// Sample 02 – Zoo
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Zoo) {
    auto captureDir = CaptureDir("zoo");

    Engine en;
    en.CreateModule<HeadlessGLFWModuleFactory>({}, HeadlessGLParams{
        .m_size       = {800, 600},
        .m_captureDir = captureDir,
    });
    en.CreateModule<OGLRendererFactory>({}, RendererParams{});
    en.CreateModule<CameraControllerModuleFactory>();

    ASSERT_FALSE(en.Startup().IsError());

    sample_zoo::SetupScene(en);

    en.Run(kFrameCount);
    en.Shutdown();

    CompareWithGoldenImage(FramePath(captureDir, kFrameCount - 1), "zoo.png");
}

// ---------------------------------------------------------------------------
// Sample 03 – Materials
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Materials) {
    auto captureDir = CaptureDir("materials");

    Engine en;
    en.CreateModule<HeadlessGLFWModuleFactory>({}, HeadlessGLParams{
        .m_size       = {800, 600},
        .m_captureDir = captureDir,
    });
    en.CreateModule<OGLRendererFactory>({}, RendererParams{});
    en.CreateModule<CameraControllerModuleFactory>();

    ASSERT_FALSE(en.Startup().IsError());

    sample_materials::SetupScene(en);

    en.Run(kFrameCount);
    en.Shutdown();

    CompareWithGoldenImage(FramePath(captureDir, kFrameCount - 1), "materials.png");
}

