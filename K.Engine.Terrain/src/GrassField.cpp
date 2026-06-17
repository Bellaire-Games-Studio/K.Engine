#include <GrassField.hpp>
#include <algorithm>
#include <cmath>

namespace KDot
{
    namespace
    {
        // Cheap, deterministic hash -> [0,1). Same inputs always give the same
        // blade, so regenerating the field is stable.
        inline float Hash(std::uint32_t x, std::uint32_t y, std::uint32_t seed)
        {
            std::uint32_t n = x * 374761393u + y * 668265263u + seed * 2246822519u;
            n = (n ^ (n >> 13)) * 1274126177u;
            n = n ^ (n >> 16);
            return static_cast<float>(n) / 4294967295.0f;
        }
    }

    void GrassField::Generate(const HeightField& field, const GrassParams& params)
    {
        m_Instances.clear();
        if (params.maxBlades <= 0 || params.density <= 0.0f)
            return;

        const float worldX = field.WorldSizeX();
        const float worldZ = field.WorldSizeZ();
        const float area = worldX * worldZ;
        if (area <= 0.0f)
            return;

        // Grid spacing from the requested density, widened if needed so the
        // hard cap spreads evenly across the whole field instead of filling a
        // corner and stopping.
        float spacing = 1.0f / std::sqrt(params.density);
        spacing = std::max(spacing, std::sqrt(area / static_cast<float>(params.maxBlades)));
        spacing = std::max(spacing, 0.25f);

        const glm::vec3 origin = field.Origin();
        const int cols = static_cast<int>(worldX / spacing);
        const int rows = static_cast<int>(worldZ / spacing);

        m_Instances.reserve(std::min(params.maxBlades, cols * rows));

        for (int j = 0; j < rows; ++j)
        {
            for (int i = 0; i < cols; ++i)
            {
                if (static_cast<int>(m_Instances.size()) >= params.maxBlades)
                    return;

                // Jitter within the cell so the grid isn't visible.
                const float jx = Hash(i, j, params.seed);
                const float jz = Hash(i, j, params.seed + 101u);
                const float wx = origin.x + (i + jx) * spacing;
                const float wz = origin.z + (j + jz) * spacing;

                const float h = field.HeightAtWorld(wx, wz);
                if (h < params.minHeight || h > params.maxHeight)
                    continue;

                const glm::vec3 n = field.NormalAtWorld(wx, wz);
                if (n.y < params.slopeThreshold)
                    continue;

                GrassInstance g;
                g.position = glm::vec3(wx, h, wz);
                g.phase    = Hash(i, j, params.seed + 202u) * 6.2831853f;
                g.height   = params.minScale + Hash(i, j, params.seed + 303u) * (params.maxScale - params.minScale);
                g.yaw      = Hash(i, j, params.seed + 404u) * 6.2831853f;
                m_Instances.push_back(g);
            }
        }
    }
}
