#include <HeightField.hpp>
#include <algorithm>

namespace KDot
{
    HeightField::HeightField(int width, int depth, float cellSize, const glm::vec3& origin)
        : m_Width(std::max(2, width)),
          m_Depth(std::max(2, depth)),
          m_CellSize(cellSize),
          m_Origin(origin),
          m_Heights(static_cast<std::size_t>(std::max(2, width)) * std::max(2, depth), 0.0f)
    {
    }

    float HeightField::GetRaw(int i, int j) const
    {
        i = std::clamp(i, 0, m_Width - 1);
        j = std::clamp(j, 0, m_Depth - 1);
        return m_Heights[Index(i, j)];
    }

    void HeightField::SetRaw(int i, int j, float h)
    {
        if (InBounds(i, j))
            m_Heights[Index(i, j)] = h;
    }

    void HeightField::AddRaw(int i, int j, float delta)
    {
        if (InBounds(i, j))
            m_Heights[Index(i, j)] += delta;
    }

    float HeightField::HeightAtWorld(float worldX, float worldZ) const
    {
        // Fractional grid coordinates.
        const float gx = (worldX - m_Origin.x) / m_CellSize;
        const float gz = (worldZ - m_Origin.z) / m_CellSize;

        const int i0 = static_cast<int>(std::floor(gx));
        const int j0 = static_cast<int>(std::floor(gz));
        const float fx = gx - i0;
        const float fz = gz - j0;

        const float h00 = GetRaw(i0,     j0);
        const float h10 = GetRaw(i0 + 1, j0);
        const float h01 = GetRaw(i0,     j0 + 1);
        const float h11 = GetRaw(i0 + 1, j0 + 1);

        const float hx0 = h00 + (h10 - h00) * fx;
        const float hx1 = h01 + (h11 - h01) * fx;
        return m_Origin.y + hx0 + (hx1 - hx0) * fz;
    }

    glm::vec3 HeightField::NormalAtWorld(float worldX, float worldZ) const
    {
        const float d = m_CellSize;
        const float hl = HeightAtWorld(worldX - d, worldZ);
        const float hr = HeightAtWorld(worldX + d, worldZ);
        const float hd = HeightAtWorld(worldX, worldZ - d);
        const float hu = HeightAtWorld(worldX, worldZ + d);
        // Gradient -> surface normal. The +y component is 2*d so steeper slopes
        // tilt the normal proportionally.
        return glm::normalize(glm::vec3(hl - hr, 2.0f * d, hd - hu));
    }

    AABB HeightField::Bounds() const
    {
        float lo = 0.0f, hi = 0.0f;
        if (!m_Heights.empty())
        {
            lo = hi = m_Heights[0];
            for (float h : m_Heights)
            {
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        }
        AABB box;
        box.min = m_Origin + glm::vec3(0.0f, lo, 0.0f);
        box.max = m_Origin + glm::vec3(WorldSizeX(), hi, WorldSizeZ());
        return box;
    }
}
