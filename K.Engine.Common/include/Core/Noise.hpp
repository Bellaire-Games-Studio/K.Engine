#pragma once
#include <glm.hpp>
#include <gtc/noise.hpp>
#include <algorithm>
#include <cmath>

namespace KDot
{
    // Fractal noise helpers layered on GLM's gradient noise. These are pure
    // functions of position + parameters, so terrain generation is fully
    // deterministic and reproducible from a seed.
    struct NoiseParams
    {
        int   octaves     = 5;
        float frequency   = 0.0035f; // base frequency (world units^-1)
        float lacunarity  = 2.0f;    // frequency multiplier per octave
        float gain        = 0.5f;    // amplitude multiplier per octave
        float amplitude   = 1.0f;    // overall amplitude
        glm::vec2 offset  = {0.0f, 0.0f}; // seed offset in noise space
    };

    namespace Noise
    {
        // Classic fractal Brownian motion. Returns roughly [-amplitude, amplitude].
        inline float FBM(glm::vec2 p, const NoiseParams& np)
        {
            p = (p + np.offset) * np.frequency;
            float sum = 0.0f, amp = 1.0f, norm = 0.0f;
            for (int i = 0; i < np.octaves; ++i)
            {
                sum  += amp * glm::simplex(p);
                norm += amp;
                p    *= np.lacunarity;
                amp  *= np.gain;
            }
            return (norm > 0.0f ? sum / norm : 0.0f) * np.amplitude;
        }

        // Ridged multifractal - sharp ridges, good for mountains/canyons.
        // Returns roughly [0, amplitude].
        inline float Ridged(glm::vec2 p, const NoiseParams& np)
        {
            p = (p + np.offset) * np.frequency;
            float sum = 0.0f, amp = 1.0f, norm = 0.0f;
            for (int i = 0; i < np.octaves; ++i)
            {
                float n = 1.0f - std::fabs(glm::simplex(p));
                n *= n; // sharpen ridges
                sum  += amp * n;
                norm += amp;
                p    *= np.lacunarity;
                amp  *= np.gain;
            }
            return (norm > 0.0f ? sum / norm : 0.0f) * np.amplitude;
        }

        // Domain-warped FBM: feed the position through noise before sampling it
        // again, producing swirling, organic terrain instead of uniform hills.
        inline float Warped(glm::vec2 p, const NoiseParams& np, float warpStrength = 40.0f)
        {
            const glm::vec2 q{
                FBM(p + glm::vec2(0.0f, 0.0f), np),
                FBM(p + glm::vec2(5.2f, 1.3f), np)};
            return FBM(p + warpStrength * q, np);
        }
    }
}
