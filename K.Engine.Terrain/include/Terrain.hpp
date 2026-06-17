#pragma once
#include <HeightField.hpp>
#include <TerrainChunk.hpp>
#include <Core/Noise.hpp>
#include <glm.hpp>
#include <vector>
#include <cstdint>

namespace KDot
{
    enum class SculptMode
    {
        Raise,   // push terrain up
        Lower,   // push terrain down
        Flatten, // pull toward the brush's average height
        Smooth,  // blur with neighbours
        Set      // pull toward an explicit target height
    };

    struct TerrainConfig
    {
        int       chunksX = 4;
        int       chunksZ = 4;
        float     cellSize = 1.0f;
        glm::vec3 origin{0.0f};
        float     heightScale = 60.0f;
        NoiseParams noise;
        bool      useRidged = false; // ridged multifractal (mountains) vs fBm
        bool      useWarp   = true;  // domain warping for organic shapes
        std::uint32_t seed  = 1337;
        TerrainChunkConfig chunk; // chunkEdge etc. (pulled from QualitySettings)
    };

    // -------------------------------------------------------------------------
    // Terrain
    //
    //  Owns the heightfield + a grid of LOD chunks. Generate() builds the field
    //  from noise; Update(camera) picks a LOD per chunk from QualitySettings and
    //  rebuilds only what changed; Sculpt() edits the field at runtime; HeightAt/
    //  NormalAt feed the physics world.
    // -------------------------------------------------------------------------
    class Terrain
    {
    public:
        Terrain() = default;

        void Generate(const TerrainConfig& cfg);
        void Update(const glm::vec3& cameraPos);

        void Sculpt(const glm::vec2& worldXZ, float radius, float strength,
                    SculptMode mode, float dt = 1.0f, float targetHeight = 0.0f);

        float     HeightAt(float x, float z) const { return m_Field.HeightAtWorld(x, z); }
        glm::vec3 NormalAt(float x, float z) const { return m_Field.NormalAtWorld(x, z); }

        const HeightField& Field() const { return m_Field; }
        HeightField&       Field()       { return m_Field; }

        const std::vector<TerrainChunk>& Chunks() const { return m_Chunks; }
        std::vector<TerrainChunk>&       Chunks()       { return m_Chunks; }

        AABB Bounds() const { return m_Field.Bounds(); }
        int  MaxLOD() const;

        // Total triangles across all currently-built chunk meshes (for stats/UI).
        std::size_t TriangleCount() const;

    private:
        float SampleNoise(float worldX, float worldZ) const;
        void  MarkDirtyInRadius(const glm::vec2& worldXZ, float radius);
        glm::vec3 ChunkCenterWorld(int chunkX, int chunkZ) const;

        TerrainConfig m_Cfg;
        HeightField   m_Field;
        std::vector<TerrainChunk> m_Chunks; // row-major: index = cz * chunksX + cx
    };
}
