#include <TerrainChunk.hpp>
#include <algorithm>
#include <cstdint>

namespace KDot
{
    namespace
    {
        // Monotonic across all chunks: every regenerated mesh gets a unique, ever-
        // increasing version, so the renderer's per-chunk GPU cache reliably
        // invalidates after a LOD change, a sculpt edit, or a full terrain rebuild.
        std::uint32_t s_NextMeshVersion = 0;
    }

    glm::vec4 TerrainChunk::ColorFor(float height, const glm::vec3& normal, const TerrainChunkConfig& cfg)
    {
        const glm::vec3 sand {0.76f, 0.70f, 0.45f};
        const glm::vec3 grass{0.28f, 0.46f, 0.18f};
        const glm::vec3 rock {0.40f, 0.36f, 0.32f};
        const glm::vec3 snow {0.92f, 0.94f, 0.97f};

        glm::vec3 base;
        if (height < cfg.sandMaxHeight)
            base = sand;
        else if (height < cfg.grassMaxHeight)
        {
            const float t = (height - cfg.sandMaxHeight) / std::max(0.001f, cfg.grassMaxHeight - cfg.sandMaxHeight);
            base = glm::mix(sand, grass, glm::clamp(t * 3.0f, 0.0f, 1.0f));
        }
        else if (height < cfg.rockMaxHeight)
        {
            const float t = (height - cfg.grassMaxHeight) / std::max(0.001f, cfg.rockMaxHeight - cfg.grassMaxHeight);
            base = glm::mix(grass, rock, glm::clamp(t, 0.0f, 1.0f));
        }
        else
            base = snow;

        // Steep faces show rock regardless of altitude.
        const float slope = glm::clamp((cfg.slopeRockThreshold - normal.y) / std::max(0.001f, cfg.slopeRockThreshold), 0.0f, 1.0f);
        base = glm::mix(base, rock, slope);

        return glm::vec4(base, 1.0f);
    }

    void TerrainChunk::Build(const HeightField& field, int lod, const TerrainChunkConfig& cfg)
    {
        if (lod == m_LOD && !m_Dirty)
            return;

        const int cells  = cfg.chunkEdge - 1;        // cells per chunk edge at LOD0
        const int stride = 1 << lod;                 // skip factor for this LOD
        const int n      = cells / stride + 1;       // vertices per edge at this LOD
        const int baseI  = m_ChunkX * cells;         // grid sample offset
        const int baseJ  = m_ChunkZ * cells;

        m_Mesh.Clear();
        m_Mesh.vertices.reserve(static_cast<std::size_t>(n) * n);
        m_Mesh.indices.reserve(static_cast<std::size_t>(n - 1) * (n - 1) * 6);

        // ---- Surface vertices --------------------------------------------------
        for (int j = 0; j < n; ++j)
        {
            for (int i = 0; i < n; ++i)
            {
                const int gi = baseI + i * stride;
                const int gj = baseJ + j * stride;
                const glm::vec3 pos = field.WorldPosition(gi, gj);
                const glm::vec3 nrm = field.NormalAtWorld(pos.x, pos.z);

                MeshVertex v;
                v.position = pos;
                v.normal   = nrm;
                v.color    = ColorFor(pos.y, nrm, cfg);
                v.texCoord = glm::vec2(static_cast<float>(i) / (n - 1), static_cast<float>(j) / (n - 1));
                v.texIndex = 0;
                m_Mesh.vertices.push_back(v);
            }
        }

        // ---- Surface indices (CCW when viewed from +Y) -------------------------
        auto idx = [n](int i, int j) { return static_cast<std::uint32_t>(j * n + i); };
        for (int j = 0; j < n - 1; ++j)
        {
            for (int i = 0; i < n - 1; ++i)
            {
                const std::uint32_t a = idx(i,     j);
                const std::uint32_t b = idx(i,     j + 1);
                const std::uint32_t c = idx(i + 1, j + 1);
                const std::uint32_t d = idx(i + 1, j);
                m_Mesh.indices.push_back(a); m_Mesh.indices.push_back(b); m_Mesh.indices.push_back(c);
                m_Mesh.indices.push_back(a); m_Mesh.indices.push_back(c); m_Mesh.indices.push_back(d);
            }
        }

        // ---- Skirts: double-sided vertical walls around the perimeter that drop
        //      below the surface, hiding gaps where a neighbour uses a coarser LOD.
        if (cfg.skirtDepth > 0.0f)
        {
            auto addWall = [&](std::uint32_t t0, std::uint32_t t1)
            {
                MeshVertex s0 = m_Mesh.vertices[t0]; s0.position.y -= cfg.skirtDepth;
                MeshVertex s1 = m_Mesh.vertices[t1]; s1.position.y -= cfg.skirtDepth;
                const std::uint32_t b0 = static_cast<std::uint32_t>(m_Mesh.vertices.size()); m_Mesh.vertices.push_back(s0);
                const std::uint32_t b1 = static_cast<std::uint32_t>(m_Mesh.vertices.size()); m_Mesh.vertices.push_back(s1);
                // front winding
                m_Mesh.indices.push_back(t0); m_Mesh.indices.push_back(t1); m_Mesh.indices.push_back(b1);
                m_Mesh.indices.push_back(t0); m_Mesh.indices.push_back(b1); m_Mesh.indices.push_back(b0);
                // back winding (so the wall is visible from either side)
                m_Mesh.indices.push_back(t0); m_Mesh.indices.push_back(b1); m_Mesh.indices.push_back(t1);
                m_Mesh.indices.push_back(t0); m_Mesh.indices.push_back(b0); m_Mesh.indices.push_back(b1);
            };

            for (int i = 0; i < n - 1; ++i) addWall(idx(i, 0),     idx(i + 1, 0));        // -Z edge
            for (int i = 0; i < n - 1; ++i) addWall(idx(i, n - 1), idx(i + 1, n - 1));    // +Z edge
            for (int j = 0; j < n - 1; ++j) addWall(idx(0, j),     idx(0, j + 1));        // -X edge
            for (int j = 0; j < n - 1; ++j) addWall(idx(n - 1, j), idx(n - 1, j + 1));    // +X edge
        }

        // ---- Bounds + center ---------------------------------------------------
        if (!m_Mesh.vertices.empty())
        {
            m_Bounds.min = m_Bounds.max = m_Mesh.vertices[0].position;
            for (const MeshVertex& v : m_Mesh.vertices)
                m_Bounds.Encapsulate(v.position);
        }
        m_Center = m_Bounds.Center();

        m_LOD = lod;
        m_Dirty = false;
        m_MeshVersion = ++s_NextMeshVersion; // mesh changed -> invalidate GPU cache
    }
}
