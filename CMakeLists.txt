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

# Default USE_D3D12 to ON on Windows, OFF on other platforms. This allows
# configuring the project to avoid pulling DirectX/D3D12 code on macOS.
if(WIN32)
    set(DEFAULT_USE_D3D12 ON)
else()
    set(DEFAULT_USE_D3D12 OFF)
endif()
set(DEFAULT_USE_OGL ON)
option(USE_D3D12 "Enable D3D12 renderer and DirectX dependencies" ${DEFAULT_USE_D3D12})
option(USE_OGL "Enable OpenGL renderer and dependencies" ${DEFAULT_USE_OGL})
message(STATUS "USE_D3D12=${USE_D3D12}")
message(STATUS "USE_OGL=${USE_OGL}")

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

# DirectX / D3D12 related packages are only needed when USE_D3D12 is enabled.
if(USE_D3D12)
    if(NOT WIN32)
        message(FATAL_ERROR "USE_D3D12 is enabled but DirectX is only available on Windows")
    endif()
    find_package(directxtk12 CONFIG REQUIRED)
    find_package(directx-headers CONFIG REQUIRED)
endif()

# TinyGLTF is header-only and doesn't provide CMake config
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

#==============================================================================
# HLSL Shader Compilation System
#==============================================================================

# Find all HLSL shader files in multiple directories
file(GLOB_RECURSE HLSL_FILES 
    "${CMAKE_SOURCE_DIR}/d3d12/shaders/*.hlsl"
)

# Helper function to compile HLSL shaders
# Parameters:
#   TARGET_NAME - The CMake target to associate the shader with
#   HLSL_FILE   - Path to the .hlsl source file
#   SHADER_TYPE - Type of shader (vs, ps, cs, gs, hs, ds)
#   ENTRY_POINT - Name of the entry point function in the shader
function(add_hlsl_shader TARGET_NAME HLSL_FILE SHADER_TYPE ENTRY_POINT)
    get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
    get_filename_component(SHADER_DIR ${HLSL_FILE} DIRECTORY)
    get_filename_component(SHADER_DIR_NAME ${SHADER_DIR} NAME)
    
    # Create a unique output name that includes the source directory
    # For custom entry points (not standard ones), include entry point name
    set(STANDARD_ENTRY_POINTS "VSMain;PSMain;CSMain;GSMain;HSMain;DSMain")
    if(ENTRY_POINT IN_LIST STANDARD_ENTRY_POINTS)
        set(COMPILED_SHADER "${CMAKE_BINARY_DIR}/d3d12/shaders/${SHADER_NAME}_${SHADER_TYPE}.cso")
    else()
        set(COMPILED_SHADER "${CMAKE_BINARY_DIR}/d3d12/shaders/${SHADER_NAME}_${SHADER_TYPE}_${ENTRY_POINT}.cso")
    endif()
    
    # Ensure output directory exists
    get_filename_component(COMPILED_SHADER_DIR ${COMPILED_SHADER} DIRECTORY)
    file(MAKE_DIRECTORY ${COMPILED_SHADER_DIR})
    
    # Find fxc.exe compiler (Windows + D3D12 only)
    if(WIN32 AND USE_D3D12)
        find_program(FXC_EXECUTABLE
            NAMES fxc.exe
            PATHS
                # Try Windows SDK paths
                "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x64"
                "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x86"
                "$ENV{WindowsSdkDir}bin/x64"
                "$ENV{WindowsSdkDir}bin/x86"
                "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
                "C:/Program Files (x86)/Windows Kits/10/bin/*/x86"
                "C:/Program Files (x86)/Windows Kits/8.1/bin/x64"
                "C:/Program Files (x86)/Windows Kits/8.1/bin/x86"
            DOC "DirectX Shader Compiler (fxc.exe)"
        )
        
        if(NOT FXC_EXECUTABLE)
            message(WARNING "fxc.exe not found. HLSL shaders will not be compiled.")
            return()
        endif()
        
        # Determine shader model (using 5.1 for D3D12 compatibility)
        set(SHADER_MODEL "${SHADER_TYPE}_5_1")
        
        # Add custom command to compile the shader
        add_custom_command(
            OUTPUT ${COMPILED_SHADER}
            COMMAND ${FXC_EXECUTABLE} 
                /T ${SHADER_MODEL}          # Target shader model
                /E ${ENTRY_POINT}           # Entry point function name
                /Fo ${COMPILED_SHADER}      # Output file
                ${HLSL_FILE}                # Input source file
            DEPENDS ${HLSL_FILE}
            COMMENT "Compiling HLSL shader: ${SHADER_DIR_NAME}/${SHADER_NAME} (${SHADER_TYPE})"
            VERBATIM
        )
        
        # Add to target sources so CMake tracks dependencies properly
        target_sources(${TARGET_NAME} PRIVATE ${COMPILED_SHADER})
        set_source_files_properties(${COMPILED_SHADER} PROPERTIES GENERATED TRUE)
    endif()
endfunction()

# Define common entry points and their associated shader types  
# Format: "EntryPointName:ShaderType"
set(SHADER_ENTRY_POINTS
    "VSMain:vs"    # Vertex Shader
    "PSMain:ps"    # Pixel Shader
    "CSMain:cs"    # Compute Shader
    "GSMain:gs"    # Geometry Shader
    "HSMain:hs"    # Hull Shader (Tessellation)
    "DSMain:ds"    # Domain Shader (Tessellation)
)

#==============================================================================
# PNG to KTX2 Converter Tool
#==============================================================================

# Standalone tool to convert PNG images to KTX2 format with mipmaps
add_executable(png2ktx tools/png2ktx.cpp lodepng.cpp)
target_link_libraries(png2ktx PRIVATE KTX::ktx)
target_include_directories(png2ktx PRIVATE ${CMAKE_SOURCE_DIR})

if(USE_D3D12)
    # Process all HLSL files and compile shaders
    set(ALL_COMPILED_SHADERS "")

    foreach(HLSL_FILE ${HLSL_FILES})
        get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
        get_filename_component(SHADER_DIR ${HLSL_FILE} DIRECTORY)
        get_filename_component(SHADER_DIR_NAME ${SHADER_DIR} NAME)
        
        message(STATUS "Processing HLSL file: ${HLSL_FILE}")
        
        # Read shader content to automatically detect entry points
        file(READ ${HLSL_FILE} SHADER_CONTENT)
        
        # Check for each type of entry point in the shader source
        foreach(ENTRY_POINT_PAIR ${SHADER_ENTRY_POINTS})
            string(REPLACE ":" ";" ENTRY_PARTS ${ENTRY_POINT_PAIR})
            list(GET ENTRY_PARTS 0 ENTRY_POINT)
            list(GET ENTRY_PARTS 1 SHADER_TYPE)
            
            # If the entry point function is found in the shader source
            if("${SHADER_CONTENT}" MATCHES "${ENTRY_POINT}")
                add_hlsl_shader(Engine ${HLSL_FILE} ${SHADER_TYPE} ${ENTRY_POINT})
                # Use the same path format as the add_hlsl_shader function
                set(COMPILED_SHADER_PATH "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}_${SHADER_TYPE}.cso")
                list(APPEND ALL_COMPILED_SHADERS ${COMPILED_SHADER_PATH})
                message(STATUS "  Found ${ENTRY_POINT} -> will compile as ${SHADER_TYPE}")
            endif()
        endforeach()
        
        # Additionally, detect compute shaders with custom entry points by looking for [numthreads(...)] attribute
        string(REGEX MATCHALL "\\[numthreads\\([^)]+\\)\\][^{]*void[ \t\r\n]+([a-zA-Z_][a-zA-Z0-9_]*)" COMPUTE_MATCHES "${SHADER_CONTENT}")
        foreach(COMPUTE_MATCH ${COMPUTE_MATCHES})
            # Extract function name from the match
            string(REGEX REPLACE ".*void[ \t\r\n]+([a-zA-Z_][a-zA-Z0-9_]*)" "\\1" COMPUTE_ENTRY_POINT "${COMPUTE_MATCH}")
            
            # Check if this entry point was already processed by the standard entry points
            set(ALREADY_PROCESSED FALSE)
            foreach(ENTRY_POINT_PAIR ${SHADER_ENTRY_POINTS})
                string(REPLACE ":" ";" ENTRY_PARTS ${ENTRY_POINT_PAIR})
                list(GET ENTRY_PARTS 0 STANDARD_ENTRY_POINT)
                if("${COMPUTE_ENTRY_POINT}" STREQUAL "${STANDARD_ENTRY_POINT}")
                    set(ALREADY_PROCESSED TRUE)
                    break()
                endif()
            endforeach()
            
            # If not already processed, add it as a compute shader
            if(NOT ALREADY_PROCESSED)
                add_hlsl_shader(Engine ${HLSL_FILE} cs ${COMPUTE_ENTRY_POINT})
                set(COMPILED_SHADER_PATH "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}_cs_${COMPUTE_ENTRY_POINT}.cso")
                list(APPEND ALL_COMPILED_SHADERS ${COMPILED_SHADER_PATH})
                message(STATUS "  Found compute shader ${COMPUTE_ENTRY_POINT} -> will compile as cs")
            endif()
        endforeach()
    endforeach()

    # Create a custom target that depends on all compiled shaders
    if(ALL_COMPILED_SHADERS)
        list(LENGTH ALL_COMPILED_SHADERS SHADER_COUNT)
        add_custom_target(CompileShaders ALL DEPENDS ${ALL_COMPILED_SHADERS})
        # add_dependencies(Engine CompileShaders)
        message(STATUS "Created CompileShaders target with ${SHADER_COUNT} shaders")
        message(STATUS "Compiled shaders will be available in: ${CMAKE_BINARY_DIR}/shaders/")
    endif()

endif() # USE_D3D12

if(USE_OGL)


endif() # USE_OGL

#==============================================================================
# Shader Preprocessor Tool (WGSL and GLSL)
#==============================================================================

# Create the shader preprocessor executable
add_executable(ShaderPreprocessor tools/shader_preprocessor.cpp)
set_target_properties(ShaderPreprocessor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

#==============================================================================
# OpenGL Shader Processing System
#==============================================================================

if (USE_OGL)
    # Find all GLSL shader files in ogl/shaders folder
    file(GLOB_RECURSE OGL_SHADER_FILES 
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.glsl"
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.vs"
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.fs"
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.gs"
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.ts"
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/ogl/shaders/*.frag"
    )

    if(OGL_SHADER_FILES)
        # Ensure ogl shaders output directory exists
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/ogl/shaders)
        
        set(ALL_OGL_PROCESSED_SHADERS "")
        
        # Process each shader file with the shader preprocessor
        foreach(OGL_SHADER_FILE ${OGL_SHADER_FILES})
            get_filename_component(SHADER_NAME ${OGL_SHADER_FILE} NAME)
            set(PROCESSED_SHADER_PATH "${CMAKE_BINARY_DIR}/ogl/shaders/${SHADER_NAME}")
            
            # Create custom command to preprocess the shader file
            # Each shader now depends on ALL shader files so touching any file regenerates all
            add_custom_command(
                OUTPUT ${PROCESSED_SHADER_PATH}
                COMMAND ShaderPreprocessor ${OGL_SHADER_FILE} ${PROCESSED_SHADER_PATH} "${CMAKE_SOURCE_DIR}/ogl/shaders"
                DEPENDS ${OGL_SHADER_FILES} ShaderPreprocessor
                COMMENT "Preprocessing OpenGL shader: ${SHADER_NAME}"
            )
            
            list(APPEND ALL_OGL_PROCESSED_SHADERS ${PROCESSED_SHADER_PATH})
            message(STATUS "Will preprocess OpenGL shader: ${SHADER_NAME}")
        endforeach()
        
        # Create a custom target that depends on all processed shaders
        if(ALL_OGL_PROCESSED_SHADERS)
            list(LENGTH ALL_OGL_PROCESSED_SHADERS OGL_SHADER_COUNT)
            add_custom_target(ProcessOpenGLShaders ALL
                DEPENDS ${ALL_OGL_PROCESSED_SHADERS}
                COMMENT "Processing all OpenGL shaders"
            )
            
            # add_dependencies(Engine ProcessOpenGLShaders)
            message(STATUS "Created ProcessOpenGLShaders target with ${OGL_SHADER_COUNT} shaders")
            message(STATUS "Processed OpenGL shaders will be available in: ${CMAKE_BINARY_DIR}/ogl/shaders/")
        endif()
    endif()

endif() # USE_OGL

#==============================================================================
# KTX Image Compilation System
#==============================================================================

# Find all image files in assets and test/assets folders
file(GLOB_RECURSE IMAGE_FILES 
    "${CMAKE_SOURCE_DIR}/assets/*.png"
    "${CMAKE_SOURCE_DIR}/assets/*.jpg"
    "${CMAKE_SOURCE_DIR}/assets/*.jpeg"
    "${CMAKE_SOURCE_DIR}/assets/*.tga"
    "${CMAKE_SOURCE_DIR}/assets/*.bmp"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.png"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.jpg"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.jpeg"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.tga"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.bmp"
)

if(IMAGE_FILES)
    # Use our custom png2ktx tool to convert PNG images to KTX2 with mipmaps
    message(STATUS "Using png2ktx tool for texture conversion")
    
    # Ensure KTX output directory exists
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/assets)
    
    set(ALL_KTX_TEXTURES "")
    set(KTX_TEXTURE_COUNT 0)
    
    # Process each PNG image file and create KTX2 compilation commands
    foreach(IMAGE_FILE ${IMAGE_FILES})
        get_filename_component(IMAGE_NAME ${IMAGE_FILE} NAME_WE)
        get_filename_component(IMAGE_EXT ${IMAGE_FILE} EXT)
        get_filename_component(IMAGE_DIR ${IMAGE_FILE} DIRECTORY)
        
        # Only process PNG files with png2ktx (it only supports PNG)
        if(IMAGE_EXT STREQUAL ".png")
            # Determine output path relative to source
            file(RELATIVE_PATH REL_DIR ${CMAKE_SOURCE_DIR} ${IMAGE_DIR})
            set(KTX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${REL_DIR}")
            set(KTX_OUTPUT_PATH "${KTX_OUTPUT_DIR}/${IMAGE_NAME}.ktx2")
            
            list(APPEND ALL_KTX_TEXTURES ${KTX_OUTPUT_PATH})
            math(EXPR KTX_TEXTURE_COUNT "${KTX_TEXTURE_COUNT} + 1")
            
            message(STATUS "Will convert ${IMAGE_NAME}.png to KTX2 with mipmaps")
            
            # Add custom command to convert this PNG to KTX2 with mipmaps
            add_custom_command(
                OUTPUT ${KTX_OUTPUT_PATH}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${KTX_OUTPUT_DIR}"
                COMMAND png2ktx ${IMAGE_FILE} ${KTX_OUTPUT_PATH} --quiet
                DEPENDS png2ktx ${IMAGE_FILE}
                COMMENT "Converting PNG to KTX2 with mipmaps: ${IMAGE_NAME}"
                VERBATIM
            )
        endif()
    endforeach()
    
    # Create a custom target that depends on all compiled textures
    if(ALL_KTX_TEXTURES)
        add_custom_target(CompileKTXTextures ALL
            DEPENDS ${ALL_KTX_TEXTURES}
            COMMENT "Converting all PNG images to KTX2 format"
        )
        
        # add_dependencies(Engine CompileKTXTextures)
        message(STATUS "Created CompileKTXTextures target with ${KTX_TEXTURE_COUNT} PNG textures")
        message(STATUS "Converted KTX2 textures will be available in: ${CMAKE_BINARY_DIR}/assets/")
        message(STATUS "Note: png2ktx generates uncompressed RGBA8 textures with full mipmap chains")
    endif()
endif()

# Link Engine executable to EngineLib (all dependencies are handled by EngineLib)
# target_link_libraries(Engine PRIVATE EngineLib)

# Create engine library

#==============================================================================
# Test Executable
#==============================================================================

# Create a library with engine sources (excluding main.cpp)
file(GLOB ENGINE_LIB_SOURCES CONFIGURE_DEPENDS 
    "*.cpp" "*.hpp" 
    "d3d12/*.cpp" "d3d12/*.hpp"
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
    set(_exclude_file FALSE)
    
    # Check D3D12 files
    if(NOT USE_D3D12)
        if("${_src}" MATCHES "/d3d12/" OR "${_fname}" MATCHES "^d3d12_")
            message(STATUS "Excluding D3D12 lib source from build: ${_src}")
            set(_exclude_file TRUE)
        endif()
    endif()
    
    # Check GLES files
    if(NOT USE_OGL)
        if("${_src}" MATCHES "/ogl/" OR "${_fname}" MATCHES "^ogl_")
            message(STATUS "Excluding OGL lib source from build: ${_src}")
            set(_exclude_file TRUE)
        endif()
    endif()
    
    # Add file to filtered list if not excluded
    if(NOT _exclude_file)
        list(APPEND _FILTERED_LIB_SOURCES ${_src})
    endif()
endforeach()
set(ENGINE_LIB_SOURCES ${_FILTERED_LIB_SOURCES})

add_library(EngineLib STATIC ${ENGINE_LIB_SOURCES})



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

if(WIN32)
    if(USE_D3D12)
        target_link_libraries(EngineLib PUBLIC
            Microsoft::DirectXTK12
            Microsoft::DirectX-Headers
            d3d12
            dxgi
            dxguid
        )
    endif()
endif()

# Set include directories for the library
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/physics)
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/d3d12)
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/ogl)
target_include_directories(EngineLib PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(EngineLib PUBLIC ${TMXLITE_INCLUDE_DIR})
target_compile_definitions(EngineLib PRIVATE NOMINMAX)
if(USE_D3D12)
    target_compile_definitions(EngineLib PUBLIC USE_D3D12=1)
endif()

# Silence OpenGL deprecation warnings on macOS
if(APPLE)
    target_compile_definitions(EngineLib PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Find all test source files
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")

# If D3D12 is disabled, exclude tests that target D3D12-specific functionality
if(NOT USE_D3D12)
    set(_FILTERED_TEST_SOURCES "")
    foreach(_ts IN LISTS TEST_SOURCES)
        get_filename_component(_tname ${_ts} NAME)
        if(NOT "${_tname}" MATCHES "^d3d12_")
            list(APPEND _FILTERED_TEST_SOURCES ${_ts})
        else()
            message(STATUS "Excluding D3D12 test from build: ${_tname}")
        endif()
    endforeach()
    set(TEST_SOURCES ${_FILTERED_TEST_SOURCES})
endif()

# Create test executable
add_executable(EngineTests ${TEST_SOURCES})

# Link test executable with engine library and gtest
target_link_libraries(EngineTests PRIVATE 
    EngineLib
    GTest::gtest
    GTest::gtest_main
)

# Set C++ standard for both targets
if (CMAKE_VERSION VERSION_GREATER 3.12)
    # set_property(TARGET Engine PROPERTY CXX_STANDARD 23)
    set_property(TARGET EngineLib PROPERTY CXX_STANDARD 23)
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

# Collect all files in config/ directory
file(GLOB CONFIG_FILES "${CMAKE_SOURCE_DIR}/config/*")

# Copy config files for tests as well
if(CONFIG_FILES)
    add_custom_command(
        TARGET EngineTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/config
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CONFIG_FILES}
            $<TARGET_FILE_DIR:EngineTests>/config/
        COMMENT "Copying config files to test output directory if newer"
    )
endif()

# Copy compiled KTX textures for tests as well
if(ALL_KTX_TEXTURES)
    add_custom_command(
        TARGET EngineTests POST_BUILD
        COMMENT "Copying KTX texture files to EngineTests output directory"
    )
    
    # Copy all generated KTX files to the appropriate EngineTests directories
    foreach(KTX_FILE ${ALL_KTX_TEXTURES})
        # Get relative path from build directory to determine target location
        file(RELATIVE_PATH KTX_REL_PATH ${CMAKE_BINARY_DIR} ${KTX_FILE})
        get_filename_component(KTX_NAME ${KTX_FILE} NAME)
        
        # Determine target path - main assets go to assets/, test assets go to tests/assets/
        if("${KTX_REL_PATH}" MATCHES "^tests/assets/")
            add_custom_command(
                TARGET EngineTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${KTX_FILE}
                    $<TARGET_FILE_DIR:EngineTests>/tests/assets/${KTX_NAME}
                COMMENT "Copying test ${KTX_NAME} to EngineTests output directory"
            )
        elseif("${KTX_REL_PATH}" MATCHES "^assets/")
            add_custom_command(
                TARGET EngineTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${KTX_FILE}
                    $<TARGET_FILE_DIR:EngineTests>/assets/${KTX_NAME}
                COMMENT "Copying ${KTX_NAME} to EngineTests output directory"
            )
        endif()
    endforeach()
endif()

# Copy .glb files and other assets to output directory
file(GLOB_RECURSE GLB_FILES 
    "${CMAKE_SOURCE_DIR}/assets/*.glb"
)
file(GLOB_RECURSE OTHER_ASSET_FILES 
    "${CMAKE_SOURCE_DIR}/assets/*.json"
    "${CMAKE_SOURCE_DIR}/assets/*.xml"
    "${CMAKE_SOURCE_DIR}/assets/*.txt"
    "${CMAKE_SOURCE_DIR}/assets/*.bin"
)

# Copy .glb files and other assets for tests as well
file(GLOB_RECURSE TEST_GLB_FILES 
    "${CMAKE_SOURCE_DIR}/tests/assets/*.glb"
)
file(GLOB_RECURSE TEST_OTHER_ASSET_FILES 
    "${CMAKE_SOURCE_DIR}/tests/assets/*.json"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.xml"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.txt"
    "${CMAKE_SOURCE_DIR}/tests/assets/*.bin"
)

# Add specific test PNG files for texture testing
list(APPEND TEST_OTHER_ASSET_FILES
    "${CMAKE_SOURCE_DIR}/tests/assets/test.png"
    "${CMAKE_SOURCE_DIR}/tests/assets/test2.png"
)

if(GLB_FILES OR OTHER_ASSET_FILES OR TEST_GLB_FILES OR TEST_OTHER_ASSET_FILES)
    add_custom_command(
        TARGET EngineTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/assets
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/tests/assets
        COMMENT "Copying asset files to test output directory"
    )
    
    # Copy main assets GLB files for tests
    foreach(GLB_FILE ${GLB_FILES})
        get_filename_component(GLB_NAME ${GLB_FILE} NAME)
        add_custom_command(
            TARGET EngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${GLB_FILE}
                $<TARGET_FILE_DIR:EngineTests>/assets/${GLB_NAME}
            COMMENT "Copying ${GLB_NAME} to test output directory"
        )
    endforeach()
    
    # Copy main assets other files for tests
    foreach(ASSET_FILE ${OTHER_ASSET_FILES})
        get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
        add_custom_command(
            TARGET EngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ASSET_FILE}
                $<TARGET_FILE_DIR:EngineTests>/assets/${ASSET_NAME}
            COMMENT "Copying ${ASSET_NAME} to test output directory"
        )
    endforeach()
    
    # Copy test-specific GLB files
    foreach(TEST_GLB_FILE ${TEST_GLB_FILES})
        get_filename_component(TEST_GLB_NAME ${TEST_GLB_FILE} NAME)
        add_custom_command(
            TARGET EngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${TEST_GLB_FILE}
                $<TARGET_FILE_DIR:EngineTests>/tests/assets/${TEST_GLB_NAME}
            COMMENT "Copying test ${TEST_GLB_NAME} to test output directory"
        )
    endforeach()
    
    # Copy test-specific other asset files
    foreach(TEST_ASSET_FILE ${TEST_OTHER_ASSET_FILES})
        get_filename_component(TEST_ASSET_NAME ${TEST_ASSET_FILE} NAME)
        add_custom_command(
            TARGET EngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${TEST_ASSET_FILE}
                $<TARGET_FILE_DIR:EngineTests>/tests/assets/${TEST_ASSET_NAME}
            COMMENT "Copying test ${TEST_ASSET_NAME} to test output directory"
        )
    endforeach()
endif()

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

add_subdirectory(samples)
