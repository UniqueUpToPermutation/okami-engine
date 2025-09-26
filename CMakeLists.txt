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
option(USE_D3D12 "Enable D3D12 renderer and DirectX dependencies" ${DEFAULT_USE_D3D12})
message(STATUS "USE_D3D12=${USE_D3D12}")

# Find packages via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-concurrentqueue CONFIG REQUIRED)
# find_package(Ktx CONFIG REQUIRED)  # Temporarily disabled due to vcpkg build issues

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
# WebGPU
#==============================================================================
# Set up wgpu-native
set(WGPU_NATIVE_VERSION "v0.19.4.1")

# Download wgpu-native if not present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native/webgpu.h")
    message(STATUS "Downloading wgpu-native...")
    if(APPLE)
        set(WGPU_PLATFORM "macos-x86_64")
        set(WGPU_ARCHIVE "wgpu-macos-x86_64-release.zip")
    elseif(WIN32)
        set(WGPU_PLATFORM "windows-x86_64")
        set(WGPU_ARCHIVE "wgpu-windows-x86_64-release.zip")
    else()
        set(WGPU_PLATFORM "linux-x86_64")
        set(WGPU_ARCHIVE "wgpu-linux-x86_64-release.zip")
    endif()
    
    file(DOWNLOAD 
        "https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_NATIVE_VERSION}/${WGPU_ARCHIVE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${WGPU_ARCHIVE}"
        SHOW_PROGRESS
    )
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_SOURCE_DIR}/${WGPU_ARCHIVE}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Move extracted files to wgpu-native directory
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native")
    file(GLOB WGPU_FILES "${CMAKE_CURRENT_SOURCE_DIR}/webgpu.h" 
                         "${CMAKE_CURRENT_SOURCE_DIR}/wgpu.h"
                         "${CMAKE_CURRENT_SOURCE_DIR}/libwgpu_native.*"
                         "${CMAKE_CURRENT_SOURCE_DIR}/wgpu_native.*"
                         "${CMAKE_CURRENT_SOURCE_DIR}/commit-sha")
    foreach(FILE ${WGPU_FILES})
        get_filename_component(FILENAME ${FILE} NAME)
        file(RENAME ${FILE} "${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native/${FILENAME}")
    endforeach()
    
    file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/${WGPU_ARCHIVE}")
endif()

# Include wgpu-native headers from wgpu-native directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native)

if(APPLE)
    set(WGPU_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native/libwgpu_native.dylib)
elseif(WIN32)
    set(WGPU_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native/wgpu_native.lib)
else()
    set(WGPU_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native/libwgpu_native.so)
endif()

#==============================================================================
# bgfx Shader Compilation System
#==============================================================================

if(USE_BGFX)
    # Find all bgfx shader files
    file(GLOB_RECURSE BGFX_SHADER_FILES 
        "${CMAKE_SOURCE_DIR}/bgfx/shaders/*.sc"
    )

    # Find shaderc executable once for all shaders
    find_program(SHADERC_EXECUTABLE
        NAMES shaderc shaderc.exe
        PATHS
            # vcpkg installed bgfx tools
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/tools/bgfx"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/tools/bgfx"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/bgfx"
            "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-osx/tools/bgfx"
            "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-linux/tools/bgfx"
            "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-windows/tools/bgfx"
            # Common bgfx installation paths
            "${CMAKE_SOURCE_DIR}/tools/bin"
            "${CMAKE_SOURCE_DIR}/bgfx/tools/bin"
            "${CMAKE_SOURCE_DIR}/../bgfx/tools/bin"
            "${CMAKE_SOURCE_DIR}/third_party/bgfx/tools/bin"
            # System paths
            "/usr/local/bin"
            "/opt/homebrew/bin"
            # Environment variable paths
            "$ENV{BGFX_DIR}/tools/bin"
            "$ENV{BGFX_ROOT}/tools/bin"
        DOC "bgfx Shader Compiler (shaderc)"
    )
    
    if(NOT SHADERC_EXECUTABLE)
        message(WARNING "shaderc not found. bgfx shaders will not be compiled.")
        message(WARNING "To fix this:")
        message(WARNING "  1. Install bgfx with tools: vcpkg install bgfx[tools]")
        message(WARNING "  2. Or set BGFX_DIR environment variable to bgfx installation")
        message(WARNING "  3. Or place shaderc executable in tools/bin/ directory")
    else()
        message(STATUS "Found shaderc: ${SHADERC_EXECUTABLE}")
    endif()

    # Function to compile bgfx shaders using shaderc
    function(add_bgfx_shader TARGET_NAME SHADER_FILE SHADER_TYPE PLATFORM)
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        get_filename_component(SHADER_DIR ${SHADER_FILE} DIRECTORY)
        
        # Create output path
        set(COMPILED_SHADER "${CMAKE_BINARY_DIR}/bgfx/shaders/${SHADER_NAME}_${PLATFORM}.bin")
        
        # Ensure output directory exists
        get_filename_component(COMPILED_SHADER_DIR ${COMPILED_SHADER} DIRECTORY)
        file(MAKE_DIRECTORY ${COMPILED_SHADER_DIR})
        
        # Find shaderc executable
        find_program(SHADERC_EXECUTABLE
            NAMES shaderc shaderc.exe
            PATHS
                # vcpkg installed bgfx tools
                "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/tools/bgfx"
                "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/tools/bgfx"
                "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/bgfx"
                "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-osx/tools/bgfx"
                "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-linux/tools/bgfx"
                "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-windows/tools/bgfx"
                # Common bgfx installation paths
                "${CMAKE_SOURCE_DIR}/tools/bin"
                "${CMAKE_SOURCE_DIR}/bgfx/tools/bin"
                "${CMAKE_SOURCE_DIR}/../bgfx/tools/bin"
                "${CMAKE_SOURCE_DIR}/third_party/bgfx/tools/bin"
                # System paths
                "/usr/local/bin"
                "/opt/homebrew/bin"
                # Environment variable paths
                "$ENV{BGFX_DIR}/tools/bin"
                "$ENV{BGFX_ROOT}/tools/bin"
            DOC "bgfx Shader Compiler (shaderc)"
        )
        
        if(NOT SHADERC_EXECUTABLE)
            message(WARNING "shaderc not found. bgfx shaders will not be compiled.")
            message(WARNING "To fix this:")
            message(WARNING "  1. Install bgfx with tools: vcpkg install bgfx[tools]")
            message(WARNING "  2. Or set BGFX_DIR environment variable to bgfx installation")
            message(WARNING "  3. Or place shaderc executable in tools/bin/ directory")
            return()
        else()
            message(STATUS "Found shaderc: ${SHADERC_EXECUTABLE}")
        endif()
        
        # Determine profile based on platform and shader type
        if(PLATFORM STREQUAL "dx11")
            if(SHADER_TYPE STREQUAL "v")
                set(PROFILE "vs_5_0")
            elseif(SHADER_TYPE STREQUAL "f")
                set(PROFILE "ps_5_0")
            endif()
        elseif(PLATFORM STREQUAL "metal")
            set(PROFILE "metal")
        elseif(PLATFORM STREQUAL "spirv")
            if(SHADER_TYPE STREQUAL "v")
                set(PROFILE "spirv")
            elseif(SHADER_TYPE STREQUAL "f")
                set(PROFILE "spirv")
            endif()
        else() # OpenGL
            if(SHADER_TYPE STREQUAL "v")
                set(PROFILE "120")
            elseif(SHADER_TYPE STREQUAL "f")
                set(PROFILE "120")
            endif()
        endif()
        
        # Add custom command to compile the shader
        add_custom_command(
            OUTPUT ${COMPILED_SHADER}
            COMMAND ${SHADERC_EXECUTABLE}
                -f ${SHADER_FILE}
                -o ${COMPILED_SHADER}
                --type ${SHADER_TYPE}
                --platform ${PLATFORM}
                --profile ${PROFILE}
                -i "${CMAKE_SOURCE_DIR}/bgfx/shaders"
                -i "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/include/bgfx"
            DEPENDS ${SHADER_FILE}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Compiling bgfx shader: ${SHADER_NAME} (${SHADER_TYPE}/${PLATFORM})"
            VERBATIM
        )
        
        # Mark shader as generated file (don't add to target sources since we use custom target)
        set_source_files_properties(${COMPILED_SHADER} PROPERTIES GENERATED TRUE)
    endfunction()

    # Detect platforms to compile for
    set(BGFX_PLATFORMS "")
    if(WIN32)
        list(APPEND BGFX_PLATFORMS "dx11")
    endif()
    if(APPLE)
        list(APPEND BGFX_PLATFORMS "metal")
    endif()
    if(UNIX AND NOT APPLE)
        list(APPEND BGFX_PLATFORMS "linux")
    endif()
    # Always add OpenGL as fallback
    list(APPEND BGFX_PLATFORMS "opengl")

    set(ALL_BGFX_COMPILED_SHADERS "")
endif() # USE_BGFX

#==============================================================================
# Main Engine Executable
#==============================================================================

# Engine executable only contains main.cpp and links to EngineLib
add_executable(Engine main.cpp)

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
    add_dependencies(Engine CompileShaders)
    message(STATUS "Created CompileShaders target with ${SHADER_COUNT} shaders")
    message(STATUS "Compiled shaders will be available in: ${CMAKE_BINARY_DIR}/shaders/")
endif()

endif() # USE_D3D12

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
    # Find toktx executable from KTX tools
    find_program(TOKTX_EXECUTABLE
        NAMES toktx toktx.exe
        PATHS
            # vcpkg installed ktx tools
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/tools/ktx"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/tools/ktx"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/ktx"
            "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-osx/tools/ktx"
            "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-linux/tools/ktx"
            "${CMAKE_CURRENT_LIST_DIR}/vcpkg_installed/x64-windows/tools/ktx"
            # System paths
            /usr/local/bin
            /usr/bin
            /opt/homebrew/bin
        NO_DEFAULT_PATH
    )

    if(TOKTX_EXECUTABLE)
        message(STATUS "Found toktx: ${TOKTX_EXECUTABLE}")
        
        # Ensure KTX output directory exists
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/assets)
        
        set(ALL_KTX_TEXTURES "")
        set(KTX_TEXTURE_COUNT 0)
        
        # Process each image file and create KTX compilation commands
        foreach(IMAGE_FILE ${IMAGE_FILES})
            get_filename_component(IMAGE_NAME ${IMAGE_FILE} NAME_WE)
            get_filename_component(IMAGE_DIR ${IMAGE_FILE} DIRECTORY)
            
            # Determine output path relative to source
            file(RELATIVE_PATH REL_DIR ${CMAKE_SOURCE_DIR} ${IMAGE_DIR})
            set(KTX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${REL_DIR}")
            set(KTX_OUTPUT_PATH "${KTX_OUTPUT_DIR}/${IMAGE_NAME}.ktx2")
            
            list(APPEND ALL_KTX_TEXTURES ${KTX_OUTPUT_PATH})
            math(EXPR KTX_TEXTURE_COUNT "${KTX_TEXTURE_COUNT} + 1")
            
            message(STATUS "Will compile ${IMAGE_NAME} to KTX2")
            
            # Add custom command to compile this specific image to KTX2
            add_custom_command(
                OUTPUT ${KTX_OUTPUT_PATH}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${KTX_OUTPUT_DIR}"
                COMMAND ${TOKTX_EXECUTABLE}
                    --t2
                    --genmipmap
                    --encode uastc
                    --uastc_quality 2
                    ${KTX_OUTPUT_PATH}
                    ${IMAGE_FILE}
                DEPENDS ${IMAGE_FILE}
                COMMENT "Compiling image to KTX2: ${IMAGE_NAME}"
                VERBATIM
            )
        endforeach()
        
        # Create a custom target that depends on all compiled textures
        if(ALL_KTX_TEXTURES)
            add_custom_target(CompileKTXTextures ALL
                DEPENDS ${ALL_KTX_TEXTURES}
                COMMENT "Compiling all images to KTX2"
            )
            
            add_dependencies(Engine CompileKTXTextures)
            message(STATUS "Created CompileKTXTextures target with ${KTX_TEXTURE_COUNT} textures")
            message(STATUS "Compiled KTX textures will be available in: ${CMAKE_BINARY_DIR}/assets/")
        endif()
    else()
        message(WARNING "toktx executable not found. Image compilation to KTX will be skipped.")
        message(STATUS "Make sure KTX tools are installed via vcpkg or system package manager.")
    endif()
endif()

# Link Engine executable to EngineLib (all dependencies are handled by EngineLib)
target_link_libraries(Engine PRIVATE EngineLib)

#==============================================================================
# Test Executable
#==============================================================================

# Create a library with engine sources (excluding main.cpp)
file(GLOB ENGINE_LIB_SOURCES CONFIGURE_DEPENDS 
    "*.cpp" "*.hpp" 
    "d3d12/*.cpp" "d3d12/*.hpp"
    "gles/*.cpp" "gles/*.hpp" 
    "glfw/*.cpp" "glfw/*.hpp"
    "webgpu/*.cpp" "webgpu/*.hpp" "webgpu/*.mm"
)
list(REMOVE_ITEM ENGINE_LIB_SOURCES "${CMAKE_SOURCE_DIR}/main.cpp")

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
    if(NOT USE_GLES)
        if("${_src}" MATCHES "/gles/" OR "${_fname}" MATCHES "^gles_")
            message(STATUS "Excluding GLES lib source from build: ${_src}")
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
        # KTX::ktx  # Temporarily disabled due to vcpkg build issues
        ${WGPU_LIBRARY}
)

# Add KTX library
find_library(KTX_LIBRARY ktx PATHS ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/lib)
if(KTX_LIBRARY)
    target_link_libraries(EngineLib PUBLIC ${KTX_LIBRARY})
    target_include_directories(EngineLib PUBLIC ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx/include)
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
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR}/d3d12)
target_include_directories(EngineLib PRIVATE ${TINYGLTF_INCLUDE_DIRS})
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
    set_property(TARGET Engine PROPERTY CXX_STANDARD 23)
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

add_custom_command(
    TARGET Engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Engine>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CONFIG_FILES}
        $<TARGET_FILE_DIR:Engine>/config/
    COMMENT "Copying config files to output directory if newer"
)

if(USE_D3D12 AND ALL_COMPILED_SHADERS)
    add_custom_command(
        TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Engine>/d3d12/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/d3d12/shaders
            $<TARGET_FILE_DIR:Engine>/d3d12/shaders
        COMMENT "Copying compiled shaders to output directory"
        DEPENDS CompileShaders
    )
endif()

# Copy compiled KTX textures to output directory
if(ALL_KTX_TEXTURES)
    add_custom_command(
        TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "KTX textures are available in build directory: ${CMAKE_BINARY_DIR}/assets/"
        COMMENT "KTX texture files are available in build directory"
    )
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

if(GLB_FILES OR OTHER_ASSET_FILES)
    add_custom_command(
        TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Engine>/assets
        COMMENT "Copying asset files to output directory"
    )
    
    # Copy GLB files
    foreach(GLB_FILE ${GLB_FILES})
        get_filename_component(GLB_NAME ${GLB_FILE} NAME)
        add_custom_command(
            TARGET Engine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${GLB_FILE}
                $<TARGET_FILE_DIR:Engine>/assets/${GLB_NAME}
            COMMENT "Copying ${GLB_NAME} to output directory"
        )
    endforeach()
    
    # Copy other asset files
    foreach(ASSET_FILE ${OTHER_ASSET_FILES})
        get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
        add_custom_command(
            TARGET Engine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ASSET_FILE}
                $<TARGET_FILE_DIR:Engine>/assets/${ASSET_NAME}
            COMMENT "Copying ${ASSET_NAME} to output directory"
        )
    endforeach()
endif()

# Copy config files for tests as well
add_custom_command(
    TARGET EngineTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CONFIG_FILES}
        $<TARGET_FILE_DIR:EngineTests>/config/
    COMMENT "Copying config files to test output directory if newer"
)

# Copy compiled KTX textures for tests as well
if(ALL_KTX_TEXTURES)
    add_custom_command(
        TARGET EngineTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "KTX textures are available in build directory: ${CMAKE_BINARY_DIR}/assets/ and ${CMAKE_BINARY_DIR}/tests/assets/"
        COMMENT "KTX texture files are available in build directory"
    )
endif()

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
