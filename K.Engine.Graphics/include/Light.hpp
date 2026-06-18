#pragma once
#include <glm.hpp>
#include <vector>

namespace KDot
{
    // Light data is plain POD + GLM so the lighting logic stays graphics-API
    // agnostic; the renderer is the only thing that turns these into uniforms.

    struct AmbientLight
    {
        glm::vec3 color{1.0f};
        float     intensity = 0.18f;
    };

    struct DirectionalLight // the "sun"; direction points the way the light travels
    {
        glm::vec3 direction{glm::normalize(glm::vec3(-0.45f, -0.85f, -0.35f))};
        glm::vec3 color{1.0f, 0.97f, 0.9f};
        float     intensity = 1.0f;
    };

    struct PointLight
    {
        glm::vec3 position{0.0f};
        glm::vec3 color{1.0f};
        float     intensity = 1.0f;
        float     radius = 60.0f; // attenuation reaches ~0 at this distance
    };

    // Holds the scene's lighting. Point lights beyond the budget set by
    // QualitySettings are culled to the nearest ones per draw.
    class LightManager
    {
    public:
        AmbientLight            ambient;
        DirectionalLight        sun;
        std::vector<PointLight> points;

        glm::vec3 fogColor{0.62f, 0.72f, 0.85f};
        float     fogDensity = 0.0006f; // exp2 fog; 0 disables

        // Return up to maxLights point lights, nearest to 'around' first.
        std::vector<PointLight> SelectPoints(const glm::vec3& around, int maxLights) const;
    };
}
