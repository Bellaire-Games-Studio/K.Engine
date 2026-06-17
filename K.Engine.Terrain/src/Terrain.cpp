#include <Terrain.hpp>
#include <Core/QualitySettings.hpp>
#include <algorithm>
#include <cmath>

namespace KDot
{
    float Terrain::SampleNoise(float worldX, float worldZ) const
    {
        const glm::vec2 p{worldX, worldZ};
        float h;
        if (m_Cfg.useRidged)
            h = Noise::Ridged(p, m_Cfg.noise);
        else if (m_Cfg.useWarp)
            h = Noise::Warped(p, m_Cfg.noise);
        else
            h = Noise::FBM(p, m_Cfg.noise);
        return h * m_Cfg.heightScale;
    }

    void Terrain::Generate(const TerrainConfig& cfg)
    {
        m_Cfg = cfg;

        // Seed the noise field deterministically.
        const float so = static_cast<float>(m_Cfg.seed) * 0.6180339887f;
        m_Cfg.noise.offset += glm::vec2(so, so * 1.7f);

        const int cells = m_Cfg.chunk.chunkEdge - 1;
        const int width = m_Cfg.chunksX * cells + 1;
        const int depth = m_Cfg.chunksZ * cells + 1;

        m_Field = HeightField(width, depth, m_Cfg.cellSize, m_Cfg.origin);

        for (int j = 0; j < depth; ++j)
        {
            for (int i = 0; i < width; ++i)
            {
                const float wx = m_Cfg.origin.x + i * m_Cfg.cellSize;
                const float wz = m_Cfg.origin.z + j * m_Cfg.cellSize;
                m_Field.SetRaw(i, j, SampleNoise(wx, wz));
            }
        }

        // Build the chunk grid; start each at the coarsest LOD so geometry exists
        // before the first Update() refines it near the camera.
        m_Chunks.clear();
        m_Chunks.reserve(static_cast<std::size_t>(m_Cfg.chunksX) * m_Cfg.chunksZ);
        const int coarse = MaxLOD();
        for (int cz = 0; cz < m_Cfg.chunksZ; ++cz)
            for (int cx = 0; cx < m_Cfg.chunksX; ++cx)
            {
                TerrainChunk chunk(cx, cz);
                chunk.Build(m_Field, coarse, m_Cfg.chunk);
                m_Chunks.push_back(std::move(chunk));
            }
    }

    int Terrain::MaxLOD() const
    {
        // Coarsest LOD allowed by the chunk's cell count (cells == 2^k).
        int cells = m_Cfg.chunk.chunkEdge - 1;
        int byCells = 0;
        while (cells > 1 && (cells % 2) == 0) { cells /= 2; ++byCells; }

        const int byQuality = std::max(0, QualitySettings::Get().maxLODLevels - 1);
        return std::min(byCells, byQuality);
    }

    glm::vec3 Terrain::ChunkCenterWorld(int chunkX, int chunkZ) const
    {
        const int cells = m_Cfg.chunk.chunkEdge - 1;
        const float wx = m_Cfg.origin.x + (chunkX * cells + cells * 0.5f) * m_Cfg.cellSize;
        const float wz = m_Cfg.origin.z + (chunkZ * cells + cells * 0.5f) * m_Cfg.cellSize;
        return glm::vec3(wx, m_Field.HeightAtWorld(wx, wz), wz);
    }

    void Terrain::Update(const glm::vec3& cameraPos)
    {
        const int maxLevels = MaxLOD() + 1;
        const QualitySettings& q = QualitySettings::Get();

        for (TerrainChunk& chunk : m_Chunks)
        {
            const glm::vec3 center = ChunkCenterWorld(chunk.ChunkX(), chunk.ChunkZ());
            const float dist = glm::distance(cameraPos, center);
            const int lod = std::min(MaxLOD(), q.SelectLOD(dist, maxLevels));
            chunk.Build(m_Field, lod, m_Cfg.chunk); // no-op unless LOD changed or dirty
        }
    }

    void Terrain::MarkDirtyInRadius(const glm::vec2& worldXZ, float radius)
    {
        const int cells = m_Cfg.chunk.chunkEdge - 1;
        const float pad = radius + m_Cfg.cellSize;

        // Affected grid-sample range.
        const float giMin = (worldXZ.x - pad - m_Cfg.origin.x) / m_Cfg.cellSize;
        const float giMax = (worldXZ.x + pad - m_Cfg.origin.x) / m_Cfg.cellSize;
        const float gjMin = (worldXZ.y - pad - m_Cfg.origin.z) / m_Cfg.cellSize;
        const float gjMax = (worldXZ.y + pad - m_Cfg.origin.z) / m_Cfg.cellSize;

        for (TerrainChunk& chunk : m_Chunks)
        {
            const int lo_i = chunk.ChunkX() * cells;
            const int hi_i = lo_i + cells;
            const int lo_j = chunk.ChunkZ() * cells;
            const int hi_j = lo_j + cells;

            const bool overlapX = static_cast<float>(lo_i) <= giMax && static_cast<float>(hi_i) >= giMin;
            const bool overlapZ = static_cast<float>(lo_j) <= gjMax && static_cast<float>(hi_j) >= gjMin;
            if (overlapX && overlapZ)
                chunk.MarkDirty();
        }
    }

    void Terrain::Sculpt(const glm::vec2& worldXZ, float radius, float strength,
                         SculptMode mode, float dt, float targetHeight)
    {
        if (radius <= 0.0f)
            return;

        const float r = radius / m_Cfg.cellSize; // radius in grid units
        const float cgx = (worldXZ.x - m_Cfg.origin.x) / m_Cfg.cellSize;
        const float cgz = (worldXZ.y - m_Cfg.origin.z) / m_Cfg.cellSize;

        const int iMin = std::max(0, static_cast<int>(std::floor(cgx - r)));
        const int iMax = std::min(m_Field.Width() - 1, static_cast<int>(std::ceil(cgx + r)));
        const int jMin = std::max(0, static_cast<int>(std::floor(cgz - r)));
        const int jMax = std::min(m_Field.Depth() - 1, static_cast<int>(std::ceil(cgz + r)));
        if (iMin > iMax || jMin > jMax)
            return;

        // Flatten needs the average height of the affected region up front.
        float average = targetHeight;
        if (mode == SculptMode::Flatten)
        {
            double sum = 0.0; int count = 0;
            for (int j = jMin; j <= jMax; ++j)
                for (int i = iMin; i <= iMax; ++i)
                {
                    const float dx = i - cgx, dz = j - cgz;
                    if (dx * dx + dz * dz <= r * r) { sum += m_Field.GetRaw(i, j); ++count; }
                }
            if (count > 0) average = static_cast<float>(sum / count);
        }

        // Smooth reads from a snapshot so the blur doesn't feed back on itself.
        std::vector<float> snapshot;
        if (mode == SculptMode::Smooth)
            snapshot = m_Field.Data();

        for (int j = jMin; j <= jMax; ++j)
        {
            for (int i = iMin; i <= iMax; ++i)
            {
                const float dx = i - cgx, dz = j - cgz;
                const float d2 = dx * dx + dz * dz;
                if (d2 > r * r)
                    continue;

                // Smooth radial falloff (1 at centre -> 0 at edge).
                const float falloff = 1.0f - (d2 / (r * r));
                const float w = falloff * falloff;
                const float amt = strength * w * dt;

                switch (mode)
                {
                    case SculptMode::Raise: m_Field.AddRaw(i, j, amt);  break;
                    case SculptMode::Lower: m_Field.AddRaw(i, j, -amt); break;
                    case SculptMode::Flatten:
                    case SculptMode::Set:
                    {
                        const float target = (mode == SculptMode::Set) ? targetHeight : average;
                        const float cur = m_Field.GetRaw(i, j);
                        m_Field.SetRaw(i, j, cur + (target - cur) * std::min(1.0f, w * std::max(strength, 0.0f) * dt));
                        break;
                    }
                    case SculptMode::Smooth:
                    {
                        auto sample = [&](int si, int sj) {
                            si = std::clamp(si, 0, m_Field.Width() - 1);
                            sj = std::clamp(sj, 0, m_Field.Depth() - 1);
                            return snapshot[static_cast<std::size_t>(sj) * m_Field.Width() + si];
                        };
                        const float avg = (sample(i - 1, j) + sample(i + 1, j) +
                                           sample(i, j - 1) + sample(i, j + 1) +
                                           sample(i, j)) * 0.2f;
                        const float cur = m_Field.GetRaw(i, j);
                        m_Field.SetRaw(i, j, cur + (avg - cur) * std::min(1.0f, w * dt));
                        break;
                    }
                }
            }
        }

        MarkDirtyInRadius(worldXZ, radius);
    }

    std::size_t Terrain::TriangleCount() const
    {
        std::size_t total = 0;
        for (const TerrainChunk& c : m_Chunks)
            total += c.Mesh().TriangleCount();
        return total;
    }
}
