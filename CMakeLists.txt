# CMakeList.txt : CMake project for Engine, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# vcpkg integration (optional, recommended)
# To use vcpkg, configure CMake with:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root>/scripts/buildsystems/vcpkg.cmake"
# Or set the VCPKG_ROOT environment variable.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Engine")

# Enable testing
enable_testing()

option(USE_OGL "Enable OpenGL renderer and dependencies" ON)
option(ASSET_BUILDER_VERBOSE "Show skipped files in AssetBuilder output (verbose mode)" OFF)
message(STATUS "USE_OGL=${USE_OGL}")
message(STATUS "ASSET_BUILDER_VERBOSE=${ASSET_BUILDER_VERBOSE}")

# Find packages via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(unofficial-im3d CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)

# tmxlite doesn't provide CMake config, find manually
find_path(TMXLITE_INCLUDE_DIR tmxlite/Map.hpp PATHS ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx/include ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/include)
find_library(TMXLITE_LIBRARY NAMES tmxlite libtmxlite tmxlite-s libtmxlite-s PATHS ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx/lib ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/lib)

# TinyGLTF is header-only and doesn't provide CMake config
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

#==============================================================================
# PNG to KTX2 Converter Tool
#==============================================================================

# Standalone tool to convert PNG/JPEG images to KTX2 format with mipmaps
add_executable(png2ktx
    tools/png2ktx.cpp
    tools/texture_processor.cpp
    tools/asset_graph.cpp
    lodepng.cpp
)
target_link_libraries(png2ktx PRIVATE KTX::ktx yaml-cpp::yaml-cpp)
target_include_directories(png2ktx PRIVATE ${CMAKE_SOURCE_DIR} ${TINYGLTF_INCLUDE_DIRS})
set_target_properties(png2ktx PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

#==============================================================================
# Shader Preprocessor Tool (GLSL)
#==============================================================================

# Create the shader preprocessor executable
add_executable(ShaderPreprocessor
    tools/shader_preprocessor.cpp
    tools/shader_processor.cpp
    tools/asset_graph.cpp
)
set_target_properties(ShaderPreprocessor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(ShaderPreprocessor PRIVATE ${CMAKE_SOURCE_DIR}/tools)
target_link_libraries(ShaderPreprocessor PRIVATE yaml-cpp::yaml-cpp)

#==============================================================================
# Asset Builder Tool
#==============================================================================

# Batch asset processor: converts textures to KTX2, preprocesses shaders, and
# copies all other files.  Only re-processes assets whose output is older than
# the input (checked at runtime, not at CMake configure time).
add_executable(AssetBuilder
    tools/asset_builder.cpp
    tools/asset_graph.cpp
    tools/texture_processor.cpp
    tools/shader_processor.cpp
    tools/geometry_processor.cpp
    lodepng.cpp
)
target_link_libraries(AssetBuilder PRIVATE KTX::ktx yaml-cpp::yaml-cpp ozz_animation_offline ozz_animation ozz_base)
target_include_directories(AssetBuilder PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/tools ${TINYGLTF_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/ozz/include)
set_target_properties(AssetBuilder PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

#==============================================================================
# Asset Processing (shaders + textures + copy) via AssetBuilder
#==============================================================================
# AssetBuilder handles incremental processing at runtime: it skips any output
# file that is already up-to-date.  We add one custom target per asset
# directory so the builder runs during every 'cmake --build' invocation.

# Helper macro: add a target that runs AssetBuilder on one directory pair.
macro(add_asset_build_target TARGET_NAME INPUT_DIR OUTPUT_DIR)
    if(ASSET_BUILDER_VERBOSE)
        set(_ab_flags "--verbose")
    else()
        set(_ab_flags "")
    endif()
    add_custom_target(${TARGET_NAME} ALL
        COMMAND AssetBuilder "${INPUT_DIR}" "${OUTPUT_DIR}" ${_ab_flags}
        DEPENDS AssetBuilder
        COMMENT "Building assets: ${INPUT_DIR} -> ${OUTPUT_DIR}"
        VERBATIM
    )
endmacro()

# Engine assets
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_asset_build_target(BuildAssets
        "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/assets"
    )
endif()

# OpenGL shaders
if(USE_OGL AND EXISTS "${CMAKE_SOURCE_DIR}/ogl/shaders")
    add_asset_build_target(BuildOGLShaders
        "${CMAKE_SOURCE_DIR}/ogl/shaders"
        "${CMAKE_BINARY_DIR}/ogl/shaders"
    )
endif()

# Link Engine executable to EngineLib (all dependencies are handled by EngineLib)
# target_link_libraries(Engine PRIVATE EngineLib)

# Create engine library

#==============================================================================
# Engine Library
#==============================================================================

# Create a library with engine sources (excluding main.cpp)
file(GLOB ENGINE_LIB_SOURCES CONFIGURE_DEPENDS 
    "*.cpp" "*.hpp" 
    "ogl/*.cpp" "ogl/*.hpp"
    "ogl/glad/*.c" "ogl/glad/*.h"
    "glfw/*.cpp" "glfw/*.hpp"
    "physics/*.cpp" "physics/*.hpp"
)
list(REMOVE_ITEM ENGINE_LIB_SOURCES "${CMAKE_SOURCE_DIR}/main.cpp")

# Remove tool files from engine library sources
file(GLOB TOOL_SOURCES "tools/*.cpp" "tools/*.hpp")
foreach(TOOL_FILE ${TOOL_SOURCES})
    list(REMOVE_ITEM ENGINE_LIB_SOURCES ${TOOL_FILE})
endforeach()

# Filter library sources based on enabled renderer options
set(_FILTERED_LIB_SOURCES "")
foreach(_src IN LISTS ENGINE_LIB_SOURCES)
    get_filename_component(_fname ${_src} NAME)
    if(NOT USE_OGL)
        if("${_src}" MATCHES "/ogl/" OR "${_fname}" MATCHES "^ogl_")
            message(STATUS "Excluding OGL lib source from build: ${_src}")
            continue()
        endif()
    endif()
    list(APPEND _FILTERED_LIB_SOURCES ${_src})
endforeach()
set(ENGINE_LIB_SOURCES ${_FILTERED_LIB_SOURCES})

add_library(EngineLib STATIC ${ENGINE_LIB_SOURCES})

if(MSVC)
    target_compile_options(EngineLib PRIVATE /bigobj)
endif()

# Link libraries for engine library
target_link_libraries(EngineLib PUBLIC
        glfw
        imgui::imgui
        glog::glog
        yaml-cpp::yaml-cpp
        KTX::ktx
        ${TMXLITE_LIBRARY}
        unofficial::im3d::im3d
        EnTT::EnTT
        ozz_animation
        ozz_base
)

target_compile_definitions(EngineLib PUBLIC ENTT_USE_ATOMIC=1)

# Add KTX library
find_library(KTX_LIBRARY ktx PATHS ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx/lib)
if(KTX_LIBRARY)
    target_link_libraries(EngineLib PUBLIC ${KTX_LIBRARY})
    target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx/include)
else()
    message(WARNING "KTX library not found")
endif()

# Set include directories for the library
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/ogl)
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/ozz/include)
target_include_directories(EngineLib PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(EngineLib PUBLIC ${TMXLITE_INCLUDE_DIR})
target_compile_definitions(EngineLib PRIVATE NOMINMAX)

# Silence OpenGL deprecation warnings on macOS
if(APPLE)
    target_compile_definitions(EngineLib PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Set C++ standard for EngineLib
if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET EngineLib PROPERTY CXX_STANDARD 23)
endif()

#==============================================================================
# Ozz animation library (core libraries only — no samples, tests, FBX, or gltf tool)
#==============================================================================
set(ozz_build_fbx     OFF CACHE BOOL "" FORCE)
set(ozz_build_gltf    OFF CACHE BOOL "" FORCE)
set(ozz_build_tools   OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos  OFF CACHE BOOL "" FORCE)
set(ozz_build_tests   OFF CACHE BOOL "" FORCE)
set(ozz_build_data    OFF CACHE BOOL "" FORCE)
set(ozz_build_msvc_rt_dll ON CACHE BOOL "" FORCE)
add_subdirectory(ozz)

add_subdirectory(tests)
add_subdirectory(samples)
add_subdirectory(tools/tests)
