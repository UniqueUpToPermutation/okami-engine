cmake_minimum_required(VERSION 3.10)

#==============================================================================
# Asset Graph Unit Tests
#
# Tests for the ResourceGraph / FileSystem / AssetProcessor pipeline.
# Uses a MockFileSystem so no real disk I/O occurs.
#==============================================================================

add_executable(AssetGraphTests
    asset_graph_test.cpp
    ${CMAKE_SOURCE_DIR}/tools/asset_graph.cpp
)

target_include_directories(AssetGraphTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(AssetGraphTests PRIVATE
    yaml-cpp::yaml-cpp
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(AssetGraphTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME AssetGraphTests COMMAND AssetGraphTests)
