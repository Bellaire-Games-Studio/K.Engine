#pragma once
#include <Shapes.hpp>

namespace KDot
{
    // Narrow-phase contact generation. Each function returns a Manifold whose
    // normal points from A toward B with the minimum translation distance in
    // 'penetration'. Resolve by pushing B along +normal / A along -normal.
    namespace Collision
    {
        Manifold AABBvsAABB(const AABB& a, const AABB& b);
        Manifold SphereVsSphere(const Sphere& a, const Sphere& b);

        // Sphere (A) versus AABB (B).
        Manifold SphereVsAABB(const Sphere& a, const AABB& b);
    }
}
