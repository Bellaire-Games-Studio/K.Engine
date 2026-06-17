#pragma once
#include <Shapes.hpp>
#include <glm.hpp>

namespace KDot
{
    // A convenience axis-aligned box collider. The physics solver works in terms
    // of world-space AABBs; this helper turns a centre + half-extents (optionally
    // offset and scaled by an owning transform) into one. Oriented boxes are a
    // planned follow-up - for now rotation is ignored, which keeps the broad/
    // narrow-phase cheap and is plenty for terrain-walking gameplay.
    class BoxCollider
    {
    public:
        BoxCollider() = default;
        BoxCollider(const glm::vec3& halfExtents, const glm::vec3& center = glm::vec3(0.0f))
            : m_HalfExtents(halfExtents), m_Center(center) {}

        const glm::vec3& HalfExtents() const { return m_HalfExtents; }
        const glm::vec3& Center()      const { return m_Center; }

        void SetHalfExtents(const glm::vec3& h) { m_HalfExtents = h; }
        void SetCenter(const glm::vec3& c)      { m_Center = c; }

        // World-space bounds for a body located at worldPosition (with an
        // optional non-uniform scale applied to the half-extents).
        AABB WorldBounds(const glm::vec3& worldPosition,
                         const glm::vec3& scale = glm::vec3(1.0f)) const
        {
            const glm::vec3 c = worldPosition + m_Center * scale;
            return AABB::FromCenterHalf(c, m_HalfExtents * scale);
        }

    private:
        glm::vec3 m_HalfExtents{0.5f};
        glm::vec3 m_Center{0.0f};
    };
}
