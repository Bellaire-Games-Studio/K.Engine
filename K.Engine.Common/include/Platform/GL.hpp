#pragma once
#include <Platform/Platform.hpp>

// -----------------------------------------------------------------------------
// Unified GL + window-system includes
//
//  Web (Emscripten): GLES3 is provided by the runtime; no loader needed.
//  Desktop: GLEW loads the GL function pointers (call glewInit() once the
//  context is current - see DesktopWindow). The engine targets GLES 3.0 / GL 3.3
//  core feature-wise so the same shaders work on both with a version swap.
// -----------------------------------------------------------------------------

#if defined(KE_PLATFORM_WEB)
    #include <emscripten.h>
    #include <emscripten/html5.h>
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
#else
    #include <GL/glew.h>
    #define GLFW_INCLUDE_NONE
    #include <GLFW/glfw3.h>
#endif
