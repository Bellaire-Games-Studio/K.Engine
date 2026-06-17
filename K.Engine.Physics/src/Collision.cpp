#include <Collision.hpp>
#include <algorithm>
#include <cmath>

namespace KDot
{
    namespace Collision
    {
        Manifold AABBvsAABB(const AABB& a, const AABB& b)
        {
            Manifold m;
            if (!a.Intersects(b))
                return m;

            // Overlap along each axis; the smallest is the separating axis.
            const glm::vec3 overlap{
                std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x),
                std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y),
                std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z)};

            int axis = 0;
            float minOverlap = overlap.x;
            if (overlap.y < minOverlap) { minOverlap = overlap.y; axis = 1; }
            if (overlap.z < minOverlap) { minOverlap = overlap.z; axis = 2; }

            const glm::vec3 dir = b.Center() - a.Center();
            m.colliding   = true;
            m.penetration = minOverlap;
            m.normal      = glm::vec3(0.0f);
            m.normal[axis] = (dir[axis] < 0.0f) ? -1.0f : 1.0f;
            return m;
        }

        Manifold SphereVsSphere(const Sphere& a, const Sphere& b)
        {
            Manifold m;
            const glm::vec3 delta = b.center - a.center;
            const float r = a.radius + b.radius;
            const float dist2 = glm::dot(delta, delta);
            if (dist2 >= r * r)
                return m;

            const float dist = std::sqrt(dist2);
            m.colliding   = true;
            m.penetration = r - dist;
            m.normal      = dist > 1e-6f ? delta / dist : glm::vec3(0.0f, 1.0f, 0.0f);
            return m;
        }

        Manifold SphereVsAABB(const Sphere& a, const AABB& b)
        {
            Manifold m;
            const glm::vec3 closest = b.ClosestPoint(a.center);
            const glm::vec3 delta   = closest - a.center;
            const float dist2 = glm::dot(delta, delta);

            if (dist2 > a.radius * a.radius)
                return m;

            if (dist2 > 1e-12f)
            {
                const float dist = std::sqrt(dist2);
                m.colliding   = true;
                m.penetration = a.radius - dist;
                m.normal      = delta / dist; // from sphere (A) toward box (B)
            }
            else
            {
                // Sphere centre is inside the box: push out along the least-deep face.
                const glm::vec3 c = b.Center();
                const glm::vec3 e = b.Extents();
                const glm::vec3 d = a.center - c;
                const glm::vec3 face{e.x - std::fabs(d.x), e.y - std::fabs(d.y), e.z - std::fabs(d.z)};

                int axis = 0;
                float minFace = face.x;
                if (face.y < minFace) { minFace = face.y; axis = 1; }
                if (face.z < minFace) { minFace = face.z; axis = 2; }

                m.colliding   = true;
                m.penetration = a.radius + minFace;
                m.normal      = glm::vec3(0.0f);
                m.normal[axis] = (d[axis] < 0.0f) ? 1.0f : -1.0f; // toward box interior from sphere
            }
            return m;
        }
    }
}
