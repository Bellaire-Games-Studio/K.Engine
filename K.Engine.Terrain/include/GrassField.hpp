#pragma once
#include <HeightField.hpp>
#include <Core/GrassInstance.hpp>
#include <vector>
#include <cstdint>

namespace KDot
{
    struct GrassParams
    {
        float density = 0.25f;        // desired blades per square world unit
        int   maxBlades = 60000;      // hard cap (spacing widens to spread these evenly)
        float minHeight = 1.5f;       // skip below this (beaches / water)
        float maxHeight = 65.0f;      // skip above this (bare rock / snow caps)
        float slopeThreshold = 0.74f; // require normal.y >= this (grass on gentler ground)
        float minScale = 0.6f;
        float maxScale = 1.5f;
        std::uint32_t seed = 1234;
    };

    // Scatters grass instances across a heightfield once on the CPU; the GPU does
    // the per-frame work (instancing + wind). Placement skips water, steep faces,
    // and high rock so blades only land on plausible ground.
    class GrassField
    {
    public:
        void Generate(const HeightField& field, const GrassParams& params);

        const std::vector<GrassInstance>& Instances() const { return m_Instances; }
        std::size_t Count() const { return m_Instances.size(); }

    private:
        std::vector<GrassInstance> m_Instances;
    };
}
