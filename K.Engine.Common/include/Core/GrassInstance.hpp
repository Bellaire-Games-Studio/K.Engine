#pragma once
#include <glm.hpp>

namespace KDot
{
    // One scattered blade/tuft of grass. The CPU only decides where blades go;
    // the GPU expands each instance into geometry and animates it. Layout is
    // kept tight and standard so it can be uploaded straight into an instance
    // vertex buffer (two vec3 attributes).
    struct GrassInstance
    {
        glm::vec3 position{0.0f}; // world-space base of the blade
        float     phase = 0.0f;   // per-blade wind phase offset
        float     height = 1.0f;  // height multiplier
        float     yaw = 0.0f;     // base orientation (radians)
    };
}
