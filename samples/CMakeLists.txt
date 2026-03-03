
# ──────────────────────────────────────────────────────────────────────────────
# Shared assets target
#
# Populates ${CMAKE_BINARY_DIR}/samples/assets/ and
#           ${CMAKE_BINARY_DIR}/samples/config/ once.
#
# All sample executables sit under subdirectories of
# ${CMAKE_BINARY_DIR}/samples/, so SearchForPath (which walks up from the exe)
# will find the shared directories without any per-sample copying.
# ──────────────────────────────────────────────────────────────────────────────

set(SHARED_ASSETS_DIR "${CMAKE_BINARY_DIR}/samples/assets")
set(SHARED_CONFIG_DIR "${CMAKE_BINARY_DIR}/samples/config")

add_custom_target(SamplesSharedAssets ALL
    COMMENT "Preparing shared sample assets and config"
)

# KTX textures must be compiled before we copy them.
if(TARGET CompileKTXTextures)
    add_dependencies(SamplesSharedAssets CompileKTXTextures)
endif()

# Ensure output directories exist.
add_custom_command(TARGET SamplesSharedAssets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHARED_ASSETS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHARED_CONFIG_DIR}"
)

# KTX textures (generated into ${CMAKE_BINARY_DIR}/assets/).
if(ALL_KTX_TEXTURES)
    foreach(KTX_FILE ${ALL_KTX_TEXTURES})
        get_filename_component(KTX_NAME ${KTX_FILE} NAME)
        add_custom_command(TARGET SamplesSharedAssets POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${KTX_FILE}"
                "${SHARED_ASSETS_DIR}/${KTX_NAME}"
            COMMENT "Copying ${KTX_NAME} to shared samples assets"
        )
    endforeach()
endif()

# GLB / binary assets from the source assets/ directory.
file(GLOB_RECURSE SHARED_GLB_FILES "${CMAKE_SOURCE_DIR}/assets/*.glb")
if(SHARED_GLB_FILES)
    add_custom_command(TARGET SamplesSharedAssets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHARED_GLB_FILES}
            "${SHARED_ASSETS_DIR}/"
    )
endif()

# Extra assets that live under samples/assets/.
file(GLOB_RECURSE SAMPLES_ASSET_FILES "${CMAKE_SOURCE_DIR}/samples/assets/*")
if(SAMPLES_ASSET_FILES)
    add_custom_command(TARGET SamplesSharedAssets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SAMPLES_ASSET_FILES}
            "${SHARED_ASSETS_DIR}/"
    )
endif()

# Config files (root config/ merged with samples/config/).
file(GLOB SHARED_CONFIG_FILES
    "${CMAKE_SOURCE_DIR}/config/*"
    "${CMAKE_SOURCE_DIR}/samples/config/*"
)
if(SHARED_CONFIG_FILES)
    add_custom_command(TARGET SamplesSharedAssets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHARED_CONFIG_FILES}
            "${SHARED_CONFIG_DIR}/"
    )
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Helper function for individual sample targets
# ──────────────────────────────────────────────────────────────────────────────

function(add_engine_sample TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    target_link_libraries(${TARGET_NAME} PRIVATE EngineLib)

    if(CMAKE_VERSION VERSION_GREATER 3.12)
        set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 23)
    endif()

    # Assets and config are in the shared directory – no per-sample copying needed.
    add_dependencies(${TARGET_NAME} SamplesSharedAssets)
endfunction()

add_subdirectory(01_hello_world)
add_subdirectory(02_zoo)
add_subdirectory(03_materials)
add_subdirectory(im3d)
add_subdirectory(imgui)