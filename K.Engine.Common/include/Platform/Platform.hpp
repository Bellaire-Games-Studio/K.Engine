#pragma once

// -----------------------------------------------------------------------------
// Platform detection
//
//  Defines exactly one of KE_PLATFORM_WEB / WINDOWS / LINUX / MACOS, plus the
//  umbrella KE_PLATFORM_DESKTOP for "any native (non-web) target". Use these
//  instead of sprinkling __EMSCRIPTEN__ / _WIN32 checks through the codebase.
// -----------------------------------------------------------------------------

#if defined(__EMSCRIPTEN__)
    #define KE_PLATFORM_WEB 1
#elif defined(_WIN32)
    #define KE_PLATFORM_WINDOWS 1
    #define KE_PLATFORM_DESKTOP 1
#elif defined(__APPLE__)
    #define KE_PLATFORM_MACOS 1
    #define KE_PLATFORM_DESKTOP 1
#elif defined(__linux__)
    #define KE_PLATFORM_LINUX 1
    #define KE_PLATFORM_DESKTOP 1
#else
    #error "K.Engine: unrecognized platform"
#endif
