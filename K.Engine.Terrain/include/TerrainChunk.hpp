#pragma once
#include <HeightField.hpp>
#include <Core/MeshData.hpp>
#include <Shapes.hpp>
#include <glm.hpp>

namespace KDot
{
    // Tunables shared by every chunk: how dense a chunk is at LOD0, how deep the
    // crack-hiding skirts hang, and the height/slope bands used to tint the
    // surface (a lightweight stand-in for full material splatting).
    struct TerrainChunkConfig
    {
        int   chunkEdge   = 65;    // vertices per edge at LOD0 (must be 2^n + 1)
        float skirtDepth  = 3.0f;  // 0 disables skirts

        float sandMaxHeight  = 2.0f;
        float grassMaxHeight = 35.0f;
        float rockMaxHeight  = 70.0f;
        float slopeRockThreshold = 0.72f; // normal.y below this => exposed rock
    };

    // One square tile of the heightfield. Rebuilds its mesh on demand whenever
    // its LOD changes or it is marked dirty by a sculpt edit.
    class TerrainChunk
    {
    public:
        TerrainChunk() = default;
        TerrainChunk(int chunkX, int chunkZ) : m_ChunkX(chunkX), m_ChunkZ(chunkZ) {}

        int  ChunkX() const { return m_ChunkX; }
        int  ChunkZ() const { return m_ChunkZ; }
        int  LOD()    const { return m_LOD; }
        bool Dirty()  const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }

        const MeshData& Mesh()   const { return m_Mesh; }
        const AABB&     Bounds() const { return m_Bounds; }
        glm::vec3       Center() const { return m_Center; }

        // (Re)generate the mesh for the given LOD. No-op if already built at that
        // LOD and not dirty.
        void Build(const HeightField& field, int lod, const TerrainChunkConfig& cfg);

    private:
        static glm::vec4 ColorFor(float height, const glm::vec3& normal, const TerrainChunkConfig& cfg);

        int  m_ChunkX = 0;
        int  m_ChunkZ = 0;
        int  m_LOD = -1;
        bool m_Dirty = true;
        MeshData  m_Mesh;
        AABB      m_Bounds;
        glm::vec3 m_Center{0.0f};
    };
}
