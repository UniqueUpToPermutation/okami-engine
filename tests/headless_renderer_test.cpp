#include <gtest/gtest.h>
#include <filesystem>
#include "../engine.hpp"
#include "../renderer.hpp"
#include "../camera.hpp"
#include "../transform.hpp"
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