#include <gtest/gtest.h>
#include <filesystem>
#include "../engine.hpp"
#include "../paths.hpp"
#include "../texture.hpp"

// Each sample exposes a Sample-derived class in scene.hpp.
#include "../samples/01_hello_world/scene.hpp"
#include "../samples/02_zoo/scene.hpp"
#include "../samples/03_materials/scene.hpp"
#include "../samples/04_sponza/scene.hpp"
#include "../samples/05_animation/scene.hpp"
#include "../samples/im3d/scene.hpp"
#include "../samples/imgui/scene.hpp"

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
        return GetExecutablePath().parent_path() / "headless_output" / testName;
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

        std::filesystem::path sourceGoldenPath =
            std::filesystem::path(OKAMI_SOURCE_DIR) / "tests" / "golden_images" / goldenName;
        std::cout << "Comparing:\n  rendered: " << renderedPath
                  << "\n   golden:  " << goldenPath << "\n";

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
            << " (max allowed: " << maxAllowedDifferences << ")\n"
            << "  To update golden, run:\n"
            << "    cp \"" << renderedPath.string() << "\" \""
            << sourceGoldenPath.string() << "\"";
    }

    // Run a sample headlessly and compare frame 0 (the initial rendered frame,
    // before any accumulated script updates) against the named golden image.
    template <typename TSample>
    void RunHeadlessSample(const char* captureName, const char* goldenName) {
        auto captureDir = CaptureDir(captureName);

        TSample sample;
        Engine en;
        sample.SetupModules(en, HeadlessGLParams{ .m_size = {800, 600}, .m_captureDir = captureDir });
        ASSERT_FALSE(en.Startup().IsError());
        sample.SetupScene(en);

        RunParams params;
        params.frameCount = sample.GetTestFrameCount();
        params.frameTime = 1.0 / 60.0; // 60 FPS
        en.Run(params);
        en.Shutdown();

        // Frame 0 is the first rendered output, before any script updates have
        // had a chance to mutate scene state, so it is deterministic regardless
        // of frame timing.
        CompareWithGoldenImage(FramePath(captureDir, sample.GetTestFrameCount()), goldenName);
    }
};

// ---------------------------------------------------------------------------
// Sample 01 – Hello World
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, HelloWorld) {
    RunHeadlessSample<sample_hello_world::HelloWorldSample>("hello_world", "hello_world.png");
}

// ---------------------------------------------------------------------------
// Sample 02 – Zoo
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Zoo) {
    RunHeadlessSample<sample_zoo::ZooSample>("zoo", "zoo.png");
}

// ---------------------------------------------------------------------------
// Sample 03 – Materials
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Materials) {
    RunHeadlessSample<sample_materials::MaterialsSample>("materials", "materials.png");
}

// ---------------------------------------------------------------------------
// Sample 04 – Sponza
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Sponza) {
    RunHeadlessSample<sample_sponza::SponzaSample>("sponza", "sponza.png");
}


// ---------------------------------------------------------------------------
// Sample 05 – Drone
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Drone) {
    RunHeadlessSample<sample_drone::DroneSample>("drone", "drone.png");
}


// ---------------------------------------------------------------------------
// Sample im3d
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, Im3d) {
    RunHeadlessSample<sample_im3d::Im3dSample>("im3d", "im3d.png");
}

// ---------------------------------------------------------------------------
// Sample imgui
// ---------------------------------------------------------------------------

TEST_F(HeadlessRendererTest, ImGui) {
    RunHeadlessSample<sample_imgui::ImGuiSample>("imgui", "imgui.png");
}

