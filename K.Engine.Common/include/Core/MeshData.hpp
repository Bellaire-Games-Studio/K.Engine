#pragma once
#include <vector>
#include <cstdint>
#include <glm.hpp>

namespace KDot
{
    // CPU-side, backend-agnostic mesh. Subsystems that generate geometry
    // (terrain, future model loaders, debug shapes) fill these out without any
    // graphics-API dependency; the renderer uploads them to a GPU buffer.
    //
    // IMPORTANT: MeshVertex intentionally mirrors KDot::Vertex in the renderer
    // field-for-field so the renderer can upload the array directly. Keep the
    // two layouts in sync.
    struct MeshVertex
    {
        glm::vec3 position{0.0f};
        glm::vec4 color{1.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec2 texCoord{0.0f};
        int       texIndex = 0;
    };

    struct MeshData
    {
        std::vector<MeshVertex>     vertices;
        std::vector<std::uint32_t>  indices;

        void Clear() { vertices.clear(); indices.clear(); }
        bool Empty() const { return indices.empty(); }
        std::size_t TriangleCount() const { return indices.size() / 3; }

        // Recompute smooth per-vertex normals from the index buffer (area-weighted).
        void RecalculateNormals()
        {
            for (auto& v : vertices)
                v.normal = glm::vec3(0.0f);

            for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                const std::uint32_t a = indices[i];
                const std::uint32_t b = indices[i + 1];
                const std::uint32_t c = indices[i + 2];
                const glm::vec3 n = glm::cross(vertices[b].position - vertices[a].position,
                                               vertices[c].position - vertices[a].position);
                vertices[a].normal += n; // unnormalized cross == area-weighted
                vertices[b].normal += n;
                vertices[c].normal += n;
            }

            for (auto& v : vertices)
            {
                const float len = glm::length(v.normal);
                v.normal = (len > 1e-8f) ? v.normal / len : glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    };
}
