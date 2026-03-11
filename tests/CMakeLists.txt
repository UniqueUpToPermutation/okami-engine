#==============================================================================
# Test Assets
#==============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/assets")
    add_asset_build_target(BuildTestAssets
        "${CMAKE_SOURCE_DIR}/tests/assets"
        "${CMAKE_BINARY_DIR}/tests/assets"
    )
endif()

#==============================================================================
# Test Executable
#==============================================================================

# Find all test source files
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Create test executable
add_executable(EngineTests ${TEST_SOURCES})

# Link test executable with engine library and gtest
target_link_libraries(EngineTests PRIVATE 
    EngineLib
    GTest::gtest
    GTest::gtest_main
)

# Set C++ standard
if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET EngineTests PROPERTY CXX_STANDARD 23)
endif()

# Register tests with CTest
add_test(NAME EngineUnitTests COMMAND EngineTests)

# Set test properties
set_tests_properties(EngineUnitTests PROPERTIES
    TIMEOUT 300  # 5 minutes timeout
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

#==============================================================================
# Post-build Steps
#==============================================================================

# Collect all files in config/ and samples/config/ directories
file(GLOB CONFIG_FILES "${CMAKE_SOURCE_DIR}/config/*")
file(GLOB SAMPLES_CONFIG_FILES "${CMAKE_SOURCE_DIR}/samples/config/*")
list(APPEND ALL_CONFIG_FILES ${CONFIG_FILES} ${SAMPLES_CONFIG_FILES})

# Copy config files for tests as well
if(ALL_CONFIG_FILES)
    add_custom_command(
        TARGET EngineTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/config
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ALL_CONFIG_FILES}
            $<TARGET_FILE_DIR:EngineTests>/config/
        COMMENT "Copying config files to test output directory if newer"
    )
endif()

# Always create the golden_images directory for test output.
# On first run, the headless renderer tests will auto-generate golden images
# here.  Commit those files to tests/golden_images/ in the source tree; they
# will be copied back on subsequent builds and the tests will compare against
# them.
add_custom_command(
    TARGET EngineTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/tests/golden_images
    COMMENT "Ensuring golden images directory exists"
)

# Copy any committed golden images from the source tree to the output dir.
# Using copy_directory so new files are picked up at build time without reconfiguring.
add_custom_command(
    TARGET EngineTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_SOURCE_DIR}/tests/golden_images"
        "$<TARGET_FILE_DIR:EngineTests>/tests/golden_images"
    COMMENT "Copying golden images to test output directory"
)

# All assets are processed by AssetBuilder targets (BuildAssets, BuildTestAssets, etc.).
# Make EngineTests depend on them explicitly so assets are ready before tests run.
foreach(_asset_target IN ITEMS BuildAssets BuildTestAssets BuildOGLShaders)
    if(TARGET ${_asset_target})
        add_dependencies(EngineTests ${_asset_target})
    endif()
endforeach()

#==============================================================================
# Custom Targets for Testing
#==============================================================================

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS EngineTests
    COMMENT "Running all tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add custom target to run tests with specific filters
add_custom_target(run_world_tests
    COMMAND EngineTests --gtest_filter="WorldTest*"
    DEPENDS EngineTests
    COMMENT "Running World tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(run_engine_tests
    COMMAND EngineTests --gtest_filter="EngineTest*"
    DEPENDS EngineTests
    COMMENT "Running Engine tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add custom target for performance tests
add_custom_target(run_performance_tests
    COMMAND EngineTests --gtest_filter="PerformanceTest*"
    DEPENDS EngineTests
    COMMENT "Running performance tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Test targets created:")
message(STATUS "  - run_tests: Run all tests")
message(STATUS "  - run_world_tests: Run World class tests only")
message(STATUS "  - run_engine_tests: Run Engine class tests only") 
message(STATUS "  - run_performance_tests: Run performance tests only")
message(STATUS "Use 'cmake --build . --target <target_name>' to run specific test suites")
