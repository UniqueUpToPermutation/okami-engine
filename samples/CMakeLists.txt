
# ──────────────────────────────────────────────────────────────────────────────
# Shared assets
#
# samples/assets/ is processed by AssetBuilder into
# ${CMAKE_BINARY_DIR}/samples/assets/ via the BuildSampleAssets target defined
# in the root CMakeLists.  Sample executables sit under subdirectories of
# ${CMAKE_BINARY_DIR}/samples/, so SearchForPath (which walks up from the exe)
# will find the shared directory without any per-sample copying.
#
# Config files are copied by the root CMakeLists post-build step.
# ──────────────────────────────────────────────────────────────────────────────

# ──────────────────────────────────────────────────────────────────────────────
# Sample assets
#
# Runs AssetBuilder on samples/assets/ -> ${CMAKE_BINARY_DIR}/samples/assets/.
# Sample executables sit under subdirectories of ${CMAKE_BINARY_DIR}/samples/,
# so SearchForPath (which walks up from the exe) finds the shared output
# directory without any per-sample copying.
# ──────────────────────────────────────────────────────────────────────────────

if(EXISTS "${CMAKE_SOURCE_DIR}/samples/assets")
    add_asset_build_target(BuildSampleAssets
        "${CMAKE_SOURCE_DIR}/samples/assets"
        "${CMAKE_BINARY_DIR}/samples/assets"
    )
endif()

function(add_engine_sample TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    target_link_libraries(${TARGET_NAME} PRIVATE EngineLib)

    if(CMAKE_VERSION VERSION_GREATER 3.12)
        set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 23)
    endif()

    # Wait for AssetBuilder to finish processing samples/assets/.
    if(TARGET BuildSampleAssets)
        add_dependencies(${TARGET_NAME} BuildSampleAssets)
    endif()
endfunction()

add_subdirectory(01_hello_world)
add_subdirectory(02_zoo)
add_subdirectory(03_materials)
add_subdirectory(04_sponza)
add_subdirectory(im3d)
add_subdirectory(imgui)