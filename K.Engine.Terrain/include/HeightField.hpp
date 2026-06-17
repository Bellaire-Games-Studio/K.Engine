#pragma once
#include <glm.hpp>
#include <Shapes.hpp>
#include <vector>
#include <cstdint>

namespace KDot
{
    // A regular grid of height samples in world space. This is the single source
    // of truth for terrain elevation: chunk meshing reads it, sculpting writes
    // it, and physics queries it for ground height/normal. Sampling is bilinear
    // so it stays smooth between grid points and independent of mesh LOD.
    class HeightField
    {
    public:
        HeightField() = default;
        HeightField(int width, int depth, float cellSize, const glm::vec3& origin = glm::vec3(0.0f));

        int   Width()    const { return m_Width; }   // sample count along X
        int   Depth()    const { return m_Depth; }   // sample count along Z
        float CellSize() const { return m_CellSize; }
        const glm::vec3& Origin() const { return m_Origin; }

        // World extent covered by the field (X and Z spans).
        float WorldSizeX() const { return (m_Width - 1) * m_CellSize; }
        float WorldSizeZ() const { return (m_Depth - 1) * m_CellSize; }

        bool InBounds(int i, int j) const
        {
            return i >= 0 && i < m_Width && j >= 0 && j < m_Depth;
        }

        std::size_t Index(int i, int j) const { return static_cast<std::size_t>(j) * m_Width + i; }

        // Raw grid access (clamped to the edge so callers never read out of range).
        float GetRaw(int i, int j) const;
        void  SetRaw(int i, int j, float h);
        void  AddRaw(int i, int j, float delta);

        std::vector<float>&       Data()       { return m_Heights; }
        const std::vector<float>& Data() const { return m_Heights; }

        // World-space world position of grid sample (i, j).
        glm::vec3 WorldPosition(int i, int j) const
        {
            return m_Origin + glm::vec3(i * m_CellSize, GetRaw(i, j), j * m_CellSize);
        }

        // Bilinear height at an arbitrary world (x, z).
        float HeightAtWorld(float worldX, float worldZ) const;
        // Smooth surface normal at an arbitrary world (x, z) via central differences.
        glm::vec3 NormalAtWorld(float worldX, float worldZ) const;

        AABB Bounds() const;

    private:
        int   m_Width = 0;
        int   m_Depth = 0;
        float m_CellSize = 1.0f;
        glm::vec3 m_Origin{0.0f};
        std::vector<float> m_Heights;
    };
}
