#pragma once
#include <glm.hpp>

namespace KDot
{
    namespace Picking
    {
        struct PickRay
        {
            glm::vec3 origin{0.0f};
            glm::vec3 direction{0.0f, 0.0f, -1.0f};
        };

        // Build a world-space ray from a normalized device coordinate.
        //   ndc.x, ndc.y in [-1, 1]  (-1,-1 = bottom-left, +1,+1 = top-right)
        // view / proj are the same matrices used to render the scene.
        inline PickRay ScreenToRay(glm::vec2 ndc, const glm::mat4& view, const glm::mat4& proj)
        {
            const glm::mat4 inv = glm::inverse(proj * view);

            glm::vec4 nearH = inv * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f); // near plane
            glm::vec4 farH  = inv * glm::vec4(ndc.x, ndc.y,  1.0f, 1.0f); // far plane

            const glm::vec3 nearP = glm::vec3(nearH) / nearH.w;
            const glm::vec3 farP  = glm::vec3(farH) / farH.w;

            PickRay r;
            r.origin    = nearP;
            r.direction = glm::normalize(farP - nearP);
            return r;
        }

        // Convenience: convert a pixel inside a viewport rectangle to NDC.
        //   pixel: cursor position relative to the viewport's top-left
        //   size : viewport size in pixels
        // Y is flipped because screen-space grows downward while NDC grows upward.
        inline glm::vec2 PixelToNDC(glm::vec2 pixel, glm::vec2 size)
        {
            return glm::vec2(
                (pixel.x / size.x) * 2.0f - 1.0f,
                1.0f - (pixel.y / size.y) * 2.0f);
        }
    }
}
