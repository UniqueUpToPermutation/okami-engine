
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

# Copy samples/config/ to the binary samples/ directory if newer.
# Sample executables sit under ${CMAKE_BINARY_DIR}/samples/<name>/ so
# SearchForPath (walking up) will find the sibling config/ automatically.
if(EXISTS "${CMAKE_SOURCE_DIR}/samples/config")
    add_custom_target(CopySampleConfig ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/samples/config"
            "${CMAKE_BINARY_DIR}/samples/config"
        COMMENT "Copying samples/config/ to build directory if newer"
    )
endif()

set(SAMPLE_COMMON_SRC "${CMAKE_CURRENT_LIST_DIR}/sample.cpp")

function(add_engine_sample TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN} ${SAMPLE_COMMON_SRC})
    target_link_libraries(${TARGET_NAME} PRIVATE EngineLib)

    if(CMAKE_VERSION VERSION_GREATER 3.12)
        set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 23)
    endif()

    # Wait for AssetBuilder to finish processing samples/assets/.
    if(TARGET BuildSampleAssets)
        add_dependencies(${TARGET_NAME} BuildSampleAssets)
    endif()

    # Ensure config/ is up-to-date before the sample runs.
    if(TARGET CopySampleConfig)
        add_dependencies(${TARGET_NAME} CopySampleConfig)
    endif()
endfunction()

add_subdirectory(01_hello_world)
add_subdirectory(02_zoo)
add_subdirectory(03_materials)
add_subdirectory(04_sponza)
add_subdirectory(05_animation)
add_subdirectory(im3d)
add_subdirectory(imgui)