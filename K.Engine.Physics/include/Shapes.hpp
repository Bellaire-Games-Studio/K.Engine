#pragma once
#include <glm.hpp>
#include <algorithm>
#include <limits>
#include <cmath>

namespace KDot
{
    // -------------------------------------------------------------------------
    // Geometric primitives + analytic ray tests. All header-only and free of
    // any graphics-API dependency so they compile on every target.
    // -------------------------------------------------------------------------

    struct Ray
    {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f}; // expected to be normalized

        glm::vec3 At(float t) const { return origin + direction * t; }
    };

    struct RaycastHit
    {
        bool      hit = false;
        float     distance = 0.0f;
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        std::uint32_t entity = ~0u; // filled in by PhysicsWorld queries
    };

    // Contact manifold; normal points from shape A toward shape B.
    struct Manifold
    {
        bool      colliding = false;
        glm::vec3 normal{0.0f};
        float     penetration = 0.0f;
    };

    struct AABB
    {
        glm::vec3 min{0.0f};
        glm::vec3 max{0.0f};

        glm::vec3 Center()  const { return 0.5f * (min + max); }
        glm::vec3 Extents() const { return 0.5f * (max - min); }
        glm::vec3 Size()    const { return max - min; }

        bool Contains(const glm::vec3& p) const
        {
            return p.x >= min.x && p.x <= max.x &&
                   p.y >= min.y && p.y <= max.y &&
                   p.z >= min.z && p.z <= max.z;
        }

        bool Intersects(const AABB& o) const
        {
            return min.x <= o.max.x && max.x >= o.min.x &&
                   min.y <= o.max.y && max.y >= o.min.y &&
                   min.z <= o.max.z && max.z >= o.min.z;
        }

        void Encapsulate(const glm::vec3& p)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        glm::vec3 ClosestPoint(const glm::vec3& p) const
        {
            return glm::clamp(p, min, max);
        }

        static AABB FromCenterHalf(const glm::vec3& c, const glm::vec3& half)
        {
            return AABB{c - half, c + half};
        }
    };

    struct Sphere
    {
        glm::vec3 center{0.0f};
        float     radius = 0.5f;

        AABB Bounds() const { return AABB::FromCenterHalf(center, glm::vec3(radius)); }
    };

    struct Plane
    {
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float     d = 0.0f; // plane: dot(normal, x) = d

        float SignedDistance(const glm::vec3& p) const { return glm::dot(normal, p) - d; }
    };

    // ---- Ray intersection tests --------------------------------------------
    inline RaycastHit RayVsAABB(const Ray& r, const AABB& box)
    {
        RaycastHit out;
        float tmin = 0.0f;
        float tmax = std::numeric_limits<float>::max();
        int   hitAxis = 0;
        float hitSign = -1.0f;

        for (int a = 0; a < 3; ++a)
        {
            const float invD = 1.0f / r.direction[a];
            float t0 = (box.min[a] - r.origin[a]) * invD;
            float t1 = (box.max[a] - r.origin[a]) * invD;
            float sign = -1.0f;
            if (invD < 0.0f) { std::swap(t0, t1); sign = 1.0f; }
            if (t0 > tmin) { tmin = t0; hitAxis = a; hitSign = sign; }
            tmax = std::min(tmax, t1);
            if (tmax < tmin)
                return out; // miss
        }

        out.hit      = true;
        out.distance = tmin;
        out.point    = r.At(tmin);
        out.normal   = glm::vec3(0.0f);
        out.normal[hitAxis] = hitSign;
        return out;
    }

    inline RaycastHit RayVsSphere(const Ray& r, const Sphere& s)
    {
        RaycastHit out;
        const glm::vec3 oc = r.origin - s.center;
        const float b = glm::dot(oc, r.direction);
        const float c = glm::dot(oc, oc) - s.radius * s.radius;
        const float disc = b * b - c;
        if (disc < 0.0f)
            return out;

        const float sq = std::sqrt(disc);
        float t = -b - sq;
        if (t < 0.0f) t = -b + sq; // origin inside sphere -> far hit
        if (t < 0.0f)
            return out;

        out.hit      = true;
        out.distance = t;
        out.point    = r.At(t);
        out.normal   = glm::normalize(out.point - s.center);
        return out;
    }

    inline RaycastHit RayVsPlane(const Ray& r, const Plane& p)
    {
        RaycastHit out;
        const float denom = glm::dot(p.normal, r.direction);
        if (std::fabs(denom) < 1e-6f)
            return out; // parallel
        const float t = (p.d - glm::dot(p.normal, r.origin)) / denom;
        if (t < 0.0f)
            return out;

        out.hit      = true;
        out.distance = t;
        out.point    = r.At(t);
        out.normal   = denom < 0.0f ? p.normal : -p.normal;
        return out;
    }
}
