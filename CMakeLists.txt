# require version 3.10 or higher
cmake_minimum_required(VERSION 3.22)

#
# Project
#
#   - dictates the output executable filename
#
project(K.Engine)

# Options you can set via command-line
option(HAS_TERMINAL "Show a terminal window for STDOUT/STDERR" ON)
option(USE_ALSA       "Force using ALSA as audio backend (Linux-only)")
option(USE_PULSEAUDIO "Force using PulseAudio as audio backend (Linux-only)")
option(USE_SDL2_MIXER "Force using SDL2_mixer as audio backend")

# Graphics backend selection. OpenGL (GLES3 on web / GL 3.3 core on desktop) is
# the implemented default; WebGPU and Vulkan are planned (see RHI/GraphicsAPI.hpp).
option(KE_BACKEND_GL     "OpenGL render backend (web + desktop)" ON)
option(KE_BACKEND_WEBGPU "WebGPU render backend (experimental)" OFF)
option(KE_BACKEND_VULKAN "Vulkan render backend (experimental)" OFF)

#
# C_CXX_SOURCES_DIR
#
#   - the place where your C/C++ source files are located
#
set(C_CXX_SOURCES_DIR_Physics "K.Engine.Physics/src")
set(C_CXX_SOURCES_DIR_Terrain "K.Engine.Terrain/src")
set(C_CXX_SOURCES_DIR_Graphics "K.Engine.Graphics/src")
set(C_CXX_SOURCES_DIR_Audio "K.Engine.Audio/src")
set(C_CXX_SOURCES_DIR_Core "K.Engine.Core/src")
set(C_CXX_SOURCES_DIR_Common "K.Engine.Common/src")
set(C_CXX_SOURCES_DIR_Editor "K.Engine.Editor/src")
set(C_CXX_SOURCES_DIR_Input "K.Engine.Input/src")
set(C_CXX_SOURCES_DIR_imgui "ImGui")
#
# C_CXX_HEADERS_DIR
#
#   - the place where your C/C++ header files are located
#
set(C_CXX_HEADERS_DIR_Physics "K.Engine.Physics/include")
set(C_CXX_HEADERS_DIR_Terrain "K.Engine.Terrain/include")
set(C_CXX_HEADERS_DIR_Graphics "K.Engine.Graphics/include")
set(C_CXX_HEADERS_DIR_Audio "K.Engine.Audio/include")
set(C_CXX_HEADERS_DIR_Core "K.Engine.Core/include")
set(C_CXX_HEADERS_DIR_Common "K.Engine.Common/include")
set(C_CXX_HEADERS_DIR_Editor "K.Engine.Editor/include")
set(C_CXX_HEADERS_DIR_Input "K.Engine.Input/include")
set(C_CXX_HEADERS_DIR_imgui "ImGui")
#
# ASSETS_DIR
#
#   - the place where your pictures, sound files, etc.. live
#
set(ASSETS_DIR "Assets")

##########################################################################
# DO NOT EDIT BELOW THIS LINE UNLESS YOU KNOW WHAT YOU ARE DOING!!       #
##########################################################################

# Set C++ Standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# output executable basename
set(OutputExecutable "${CMAKE_PROJECT_NAME}")

######################################################################
# Directories

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# We need to specify the output for each configuration to make it work
# on Visual Studio solutions.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_PROFILE "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_PROFILE "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_PROFILE "${CMAKE_BINARY_DIR}/bin")

set(SOURCE_DATA_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/${ASSETS_DIR})
set(SOURCE_CXX_INCLUDE_DIR_Physics ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Physics})
set(SOURCE_CXX_INCLUDE_DIR_Terrain ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Terrain})
set(SOURCE_CXX_INCLUDE_DIR_Graphics ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Graphics})
set(SOURCE_CXX_INCLUDE_DIR_Audio ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Audio})
set(SOURCE_CXX_INCLUDE_DIR_Core ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Core})
set(SOURCE_CXX_INCLUDE_DIR_Common ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Common})
set(SOURCE_CXX_INCLUDE_DIR_Editor ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Editor})
set(SOURCE_CXX_INCLUDE_DIR_Input ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_Input})
set(IMGUI                          ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_HEADERS_DIR_imgui})
set(glm                           ${CMAKE_CURRENT_SOURCE_DIR}/glm)          

set(SOURCE_CXX_SRC_DIR_Physics     ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Physics})
set(SOURCE_CXX_SRC_DIR_Terrain     ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Terrain})
set(SOURCE_CXX_SRC_DIR_Graphics    ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Graphics})
set(SOURCE_CXX_SRC_DIR_Audio       ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Audio})
set(SOURCE_CXX_SRC_DIR_Core        ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Core})
set(SOURCE_CXX_SRC_DIR_Common      ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Common})
set(SOURCE_CXX_SRC_DIR_Editor      ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Editor})
set(SOURCE_CXX_SRC_DIR_Input       ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_Input})
set(SOURCE_CXX_SRC_DIR_imgui       ${CMAKE_CURRENT_SOURCE_DIR}/${C_CXX_SOURCES_DIR_imgui})


# User C++ behaviours authored in the editor live here; they are globbed in and
# compiled alongside the engine (re-run CMake after adding a new script file).
set(SOURCE_CXX_SRC_DIR_Scripts ${CMAKE_CURRENT_SOURCE_DIR}/Scripts)

# Source Files are Curated Here
file(
    GLOB_RECURSE SOURCE_CXX_FILES
    "${SOURCE_CXX_SRC_DIR_Physics}/*.cpp" "${SOURCE_CXX_SRC_DIR_Terrain}/*.cpp" "${SOURCE_CXX_SRC_DIR_Graphics}/*.cpp" "${SOURCE_CXX_SRC_DIR_imgui}/*.cpp" "${SOURCE_CXX_SRC_DIR_Audio}/*.cpp" "${SOURCE_CXX_SRC_DIR_Core}/*.cpp" "${SOURCE_CXX_SRC_DIR_Common}/*.cpp" "${SOURCE_CXX_SRC_DIR_Editor}/*.cpp" "${SOURCE_CXX_SRC_DIR_Input}/*.cpp" "${SOURCE_CXX_SRC_DIR_Scripts}/*.cpp"
)

# Include Directories

# Search in the "cmake" directory for additional CMake modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Executable aka binary output
add_executable(${OutputExecutable} ${SOURCE_CXX_FILES})
# Include headers



######################################################################
# Emscripten
######################################################################
if (EMSCRIPTEN)
    
    # Generate an HTML file
    set(CMAKE_EXECUTABLE_SUFFIX .html)

    # Build Cache: SDL2_mixer, libpng, zlib
    execute_process(COMMAND "${EMSCRIPTEN_ROOT_PATH}/embuilder${EMCC_SUFFIX}" build sdl2_mixer libpng zlib)
    
    if(EXISTS "${SOURCE_DATA_DIR}" AND IS_DIRECTORY "${SOURCE_DATA_DIR}")
        target_link_options(
            ${OutputExecutable}
            PRIVATE
            -O2
            --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/editor.html
            -sALLOW_MEMORY_GROWTH=1
            -sMAX_WEBGL_VERSION=2
            -sMIN_WEBGL_VERSION=2
            -sUSE_LIBPNG=1
            -sUSE_GLFW=3
            -sFULL_ES3=1
            -sLLD_REPORT_UNDEFINED
            --preload-file ${SOURCE_DATA_DIR}@Assets)
    else()
        target_link_options(
            ${OutputExecutable}
            PRIVATE
            -O2
            --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/editor.html
            -sALLOW_MEMORY_GROWTH=1
            -sMAX_WEBGL_VERSION=2
            -sMIN_WEBGL_VERSION=2
            -sUSE_LIBPNG=1
            -sFULL_ES3=1
            -sUSE_GLFW=3 
            -sLLD_REPORT_UNDEFINED
            --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
    endif()

endif() # Emscripten


######################################################################
# Set include directory
######################################################################
target_include_directories(${OutputExecutable} PRIVATE ${IMGUI} ${glm} ${SOURCE_CXX_INCLUDE_DIR_Physics} ${SOURCE_CXX_INCLUDE_DIR_Terrain} ${SOURCE_CXX_INCLUDE_DIR_Graphics} ${SOURCE_CXX_INCLUDE_DIR_Audio} ${SOURCE_CXX_INCLUDE_DIR_Core} ${SOURCE_CXX_INCLUDE_DIR_Common} ${SOURCE_CXX_INCLUDE_DIR_Editor} ${SOURCE_CXX_INCLUDE_DIR_Input})


######################################################################
# Backend defines
######################################################################
if (KE_BACKEND_WEBGPU)
    target_compile_definitions(${OutputExecutable} PRIVATE KE_BACKEND_WEBGPU)
    # Experimental WebGPU scaffold (RHI/WebGPUDevice). Pulls in Emscripten's
    # webgpu.h on the web target. NOTE: not a runnable backend yet - the rest of
    # the renderer + ImGui are still OpenGL (see docs/WEBGPU_PORT.md).
    if (EMSCRIPTEN)
        target_link_options(${OutputExecutable} PRIVATE -sUSE_WEBGPU=1)
    endif()
endif()
if (KE_BACKEND_VULKAN)
    target_compile_definitions(${OutputExecutable} PRIVATE KE_BACKEND_VULKAN)
endif()


######################################################################
# Native desktop (Windows / Linux / macOS) link dependencies
#
#   The web build gets GL + GLFW from Emscripten; native builds link real
#   OpenGL, GLEW (function loader) and GLFW. Install them first:
#     Linux  : sudo apt install libglfw3-dev libglew-dev
#     macOS  : brew install glfw glew
#     Windows: vcpkg install glfw3 glew   (use the vcpkg toolchain file)
######################################################################
if (NOT EMSCRIPTEN)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 QUIET)
    find_package(GLEW QUIET)

    if (NOT TARGET glfw)
        message(FATAL_ERROR "K.Engine desktop build requires GLFW3 (target 'glfw'). "
                            "Install it (Linux: libglfw3-dev, macOS: brew install glfw, "
                            "Windows: vcpkg install glfw3) or set CMAKE_PREFIX_PATH.")
    endif()
    if (NOT GLEW_FOUND)
        message(FATAL_ERROR "K.Engine desktop build requires GLEW. "
                            "Install it (Linux: libglew-dev, macOS: brew install glew, "
                            "Windows: vcpkg install glew).")
    endif()

    target_link_libraries(${OutputExecutable} PRIVATE OpenGL::GL GLEW::GLEW glfw)

    if (UNIX AND NOT APPLE)
        find_package(Threads REQUIRED)
        target_link_libraries(${OutputExecutable} PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
    endif()
endif()


######################################################################
# Copy assets/ directory target
######################################################################

set(DATA_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/${ASSETS_DIR})

file(GLOB_RECURSE src_data_files
    RELATIVE ${SOURCE_DATA_DIR}/ "${SOURCE_DATA_DIR}/*.*" "${SOURCE_DATA_DIR}/*")
foreach(fn ${src_data_files})
    add_custom_command(
        OUTPUT ${DATA_OUTPUT_DIR}/${fn}
        COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DATA_DIR}/${fn} ${DATA_OUTPUT_DIR}/${fn}
        MAIN_DEPENDENCY ${SOURCE_DATA_DIR}/${fn})
    list(APPEND out_data_files ${DATA_OUTPUT_DIR}/${fn})
endforeach()

add_custom_target(copy_data DEPENDS ${out_data_files})

# Copy Asset Files, if not Emscripten
if (NOT EMSCRIPTEN)
    add_dependencies(${OutputExecutable} copy_data)
endif()