#pragma once

// -----------------------------------------------------------------------------
// Render Hardware Interface (RHI) - backend selection
//
//  K.Engine currently renders through OpenGL: GLES3 on the web (WebGL2) and
//  GL 3.3 core on desktop, behind one set of shaders (see ShaderUtil). This
//  header is the seam where additional GPU backends slot in.
//
//  Roadmap:
//    * OpenGL  - implemented (Renderer, GrassRenderer).  [web + desktop]
//    * WebGPU  - planned: Dawn (desktop) / browser WebGPU (web) via a common
//                WGSL pipeline. Portable, modern, the natural web successor.
//    * Vulkan  - planned: desktop high-performance path (best for native GPUs).
//
//  Backends are selected at build time (see the KE_BACKEND_* CMake options) and,
//  eventually, at runtime via GraphicsAPI below. The renderer will be refactored
//  to talk to an abstract device interface so the GL code becomes one backend
//  among several rather than the only path.
// -----------------------------------------------------------------------------

namespace KDot
{
    enum class GraphicsAPI
    {
        OpenGL, // GLES3 (web) / GL 3.3 core (desktop) - current default
        WebGPU, // planned
        Vulkan  // planned
    };

    // The backend the build was configured with. Defaults to OpenGL until the
    // other backends land (the KE_BACKEND_* macros are set by CMake).
    constexpr GraphicsAPI kActiveGraphicsAPI =
#if defined(KE_BACKEND_VULKAN)
        GraphicsAPI::Vulkan;
#elif defined(KE_BACKEND_WEBGPU)
        GraphicsAPI::WebGPU;
#else
        GraphicsAPI::OpenGL;
#endif

    inline const char* GraphicsAPIName(GraphicsAPI api)
    {
        switch (api)
        {
            case GraphicsAPI::OpenGL: return "OpenGL";
            case GraphicsAPI::WebGPU: return "WebGPU";
            case GraphicsAPI::Vulkan: return "Vulkan";
        }
        return "Unknown";
    }
}
