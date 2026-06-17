#include <PhysicsWorld.hpp>

#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace KDot
{
    namespace
    {
        // Pack a 3D integer cell coordinate into one 64-bit spatial-hash key.
        inline std::int64_t CellKey(int x, int y, int z)
        {
            const std::int64_t X = static_cast<std::int64_t>(x) & 0x1FFFFF;
            const std::int64_t Y = static_cast<std::int64_t>(y) & 0x1FFFFF;
            const std::int64_t Z = static_cast<std::int64_t>(z) & 0x1FFFFF;
            return X | (Y << 21) | (Z << 42);
        }

        inline std::uint64_t PairKey(int i, int j)
        {
            if (i > j) std::swap(i, j);
            return (static_cast<std::uint64_t>(i) << 32) | static_cast<std::uint32_t>(j);
        }
    }

    AABB PhysicsWorld::WorldAABB(const Transform& t, const Collider& c) const
    {
        if (c.type == ColliderType::Sphere)
            return WorldSphere(t, c).Bounds();
        const glm::vec3 center = t.position + c.localOffset * t.scale;
        return AABB::FromCenterHalf(center, c.halfExtents * t.scale);
    }

    Sphere PhysicsWorld::WorldSphere(const Transform& t, const Collider& c) const
    {
        Sphere s;
        s.center = t.position + c.localOffset * t.scale;
        s.radius = c.radius * std::max(std::max(t.scale.x, t.scale.y), t.scale.z);
        return s;
    }

    Manifold PhysicsWorld::ComputeManifold(const Proxy& a, const Proxy& b) const
    {
        const bool aBox = a.collider->type == ColliderType::Box;
        const bool bBox = b.collider->type == ColliderType::Box;

        if (aBox && bBox)
            return Collision::AABBvsAABB(a.bounds, b.bounds);

        if (!aBox && !bBox)
            return Collision::SphereVsSphere(WorldSphere(*a.transform, *a.collider),
                                             WorldSphere(*b.transform, *b.collider));

        if (!aBox && bBox)
            return Collision::SphereVsAABB(WorldSphere(*a.transform, *a.collider), b.bounds);

        // box (a) vs sphere (b): compute sphere-vs-box, then flip the normal so
        // it still points from A toward B.
        Manifold m = Collision::SphereVsAABB(WorldSphere(*b.transform, *b.collider), a.bounds);
        m.normal = -m.normal;
        return m;
    }

    void PhysicsWorld::Step(ecs::Registry& registry, float dt)
    {
        if (dt <= 0.0f)
            return;

        m_Proxies.clear();
        registry.View<Transform, Rigidbody, Collider>(
            [this](ecs::Entity e, Transform& t, Rigidbody& rb, Collider& c)
            {
                m_Proxies.push_back(Proxy{e, &t, &rb, &c, AABB{}});
            });

        Integrate(dt);
        ResolveTerrain();
        ResolveCollisions();
    }

    void PhysicsWorld::Integrate(float dt)
    {
        for (Proxy& p : m_Proxies)
        {
            Rigidbody& rb = *p.body;
            if (rb.isStatic || rb.invMass == 0.0f)
            {
                rb.onGround = false;
                rb.accumulatedForce = glm::vec3(0.0f);
                continue;
            }

            if (rb.useGravity)
                rb.velocity += gravity * dt;
            rb.velocity += rb.accumulatedForce * rb.invMass * dt;
            rb.velocity *= std::max(0.0f, 1.0f - rb.linearDamping * dt);

            p.transform->position += rb.velocity * dt;

            rb.onGround = false;
            rb.accumulatedForce = glm::vec3(0.0f);
        }
    }

    void PhysicsWorld::ResolveTerrain()
    {
        if (!sampleTerrainHeight)
            return;

        for (Proxy& p : m_Proxies)
        {
            Rigidbody& rb = *p.body;
            if (rb.isStatic || rb.invMass == 0.0f)
                continue;

            const glm::vec3 center = p.transform->position + p.collider->localOffset * p.transform->scale;
            const float bottom = (p.collider->type == ColliderType::Sphere)
                                     ? center.y - p.collider->radius * p.transform->scale.y
                                     : center.y - p.collider->halfExtents.y * p.transform->scale.y;

            const float ground = sampleTerrainHeight(center.x, center.z);
            if (bottom < ground)
            {
                p.transform->position.y += (ground - bottom);

                if (rb.velocity.y < 0.0f)
                    rb.velocity.y = (rb.restitution > 0.05f) ? -rb.velocity.y * rb.restitution : 0.0f;

                // Ground friction on the horizontal velocity.
                const float keep = std::max(0.0f, 1.0f - rb.friction);
                rb.velocity.x *= keep;
                rb.velocity.z *= keep;
                rb.onGround = true;
            }
        }
    }

    void PhysicsWorld::ResolveCollisions()
    {
        const std::size_t n = m_Proxies.size();
        if (n < 2)
            return;

        // Refresh world bounds after integration / terrain clamping.
        for (Proxy& p : m_Proxies)
            p.bounds = WorldAABB(*p.transform, *p.collider);

        // ---- Broadphase: spatial hash -> unique candidate pairs --------------
        const float inv = 1.0f / std::max(0.001f, broadphaseCellSize);
        std::unordered_map<std::int64_t, std::vector<int>> grid;
        grid.reserve(n * 2);

        auto cellOf = [inv](float v) { return static_cast<int>(std::floor(v * inv)); };

        for (int i = 0; i < static_cast<int>(n); ++i)
        {
            const AABB& b = m_Proxies[i].bounds;
            for (int x = cellOf(b.min.x); x <= cellOf(b.max.x); ++x)
                for (int y = cellOf(b.min.y); y <= cellOf(b.max.y); ++y)
                    for (int z = cellOf(b.min.z); z <= cellOf(b.max.z); ++z)
                        grid[CellKey(x, y, z)].push_back(i);
        }

        std::unordered_set<std::uint64_t> seen;
        std::vector<std::pair<int, int>> pairs;
        for (auto& cell : grid)
        {
            std::vector<int>& bucket = cell.second;
            for (std::size_t a = 0; a < bucket.size(); ++a)
                for (std::size_t b = a + 1; b < bucket.size(); ++b)
                {
                    const int i = bucket[a];
                    const int j = bucket[b];
                    const Rigidbody& ra = *m_Proxies[i].body;
                    const Rigidbody& rb = *m_Proxies[j].body;
                    if (ra.invMass == 0.0f && rb.invMass == 0.0f)
                        continue;
                    if (!m_Proxies[i].bounds.Intersects(m_Proxies[j].bounds))
                        continue;
                    if (seen.insert(PairKey(i, j)).second)
                        pairs.emplace_back(i, j);
                }
        }

        // ---- Narrow-phase + sequential impulse resolution --------------------
        for (int iter = 0; iter < solverIterations; ++iter)
        {
            for (auto& pr : pairs)
            {
                Proxy& A = m_Proxies[pr.first];
                Proxy& B = m_Proxies[pr.second];

                // Bounds may have shifted from a previous iteration's correction.
                A.bounds = WorldAABB(*A.transform, *A.collider);
                B.bounds = WorldAABB(*B.transform, *B.collider);

                const Manifold m = ComputeManifold(A, B);
                if (!m.colliding)
                    continue;
                if (A.collider->isTrigger || B.collider->isTrigger)
                    continue; // overlap only; no physical response

                Rigidbody& a = *A.body;
                Rigidbody& b = *B.body;
                const float invSum = a.invMass + b.invMass;
                if (invSum <= 0.0f)
                    continue;

                glm::vec3 rv = b.velocity - a.velocity;
                const float velAlongN = glm::dot(rv, m.normal);
                if (velAlongN < 0.0f) // bodies approaching
                {
                    const float e = std::min(a.restitution, b.restitution);
                    const float j = -(1.0f + e) * velAlongN / invSum;
                    const glm::vec3 impulse = j * m.normal;
                    a.velocity -= impulse * a.invMass;
                    b.velocity += impulse * b.invMass;

                    // Coulomb friction along the tangent.
                    rv = b.velocity - a.velocity;
                    glm::vec3 tangent = rv - glm::dot(rv, m.normal) * m.normal;
                    const float tl = glm::length(tangent);
                    if (tl > 1e-6f)
                    {
                        tangent /= tl;
                        const float jt = -glm::dot(rv, tangent) / invSum;
                        const float mu = std::sqrt(a.friction * b.friction);
                        const glm::vec3 frictionImpulse =
                            (std::fabs(jt) < j * mu) ? jt * tangent : -j * mu * tangent;
                        a.velocity -= frictionImpulse * a.invMass;
                        b.velocity += frictionImpulse * b.invMass;
                    }
                }

                // Baumgarte positional correction to fight sinking.
                const float corr = std::max(m.penetration - penetrationSlop, 0.0f) / invSum * positionalCorrection;
                const glm::vec3 c = corr * m.normal;
                A.transform->position -= c * a.invMass;
                B.transform->position += c * b.invMass;
            }
        }
    }

    RaycastHit PhysicsWorld::RaycastTerrain(const Ray& ray, float maxDistance) const
    {
        RaycastHit out;
        if (!sampleTerrainHeight)
            return out;

        const float step = std::max(0.5f, broadphaseCellSize * 0.5f);
        float prevT = 0.0f;
        float prevDiff = ray.origin.y - sampleTerrainHeight(ray.origin.x, ray.origin.z);

        for (float t = step; t <= maxDistance; t += step)
        {
            const glm::vec3 p = ray.At(t);
            const float diff = p.y - sampleTerrainHeight(p.x, p.z);
            if (diff <= 0.0f && prevDiff > 0.0f)
            {
                float lo = prevT, hi = t;
                for (int i = 0; i < 8; ++i) // bisect onto the surface
                {
                    const float mid = 0.5f * (lo + hi);
                    const glm::vec3 pm = ray.At(mid);
                    if (pm.y - sampleTerrainHeight(pm.x, pm.z) > 0.0f)
                        lo = mid;
                    else
                        hi = mid;
                }
                const float th = 0.5f * (lo + hi);
                const glm::vec3 ph = ray.At(th);
                out.hit = true;
                out.distance = th;
                out.point = ph;
                out.normal = sampleTerrainNormal ? sampleTerrainNormal(ph.x, ph.z) : glm::vec3(0.0f, 1.0f, 0.0f);
                return out;
            }
            prevDiff = diff;
            prevT = t;
        }
        return out;
    }

    RaycastHit PhysicsWorld::Raycast(ecs::Registry& registry, const Ray& ray, float maxDistance)
    {
        m_Proxies.clear();
        registry.View<Transform, Rigidbody, Collider>(
            [this](ecs::Entity e, Transform& t, Rigidbody& rb, Collider& c)
            {
                m_Proxies.push_back(Proxy{e, &t, &rb, &c, WorldAABB(t, c)});
            });

        RaycastHit best;
        best.distance = maxDistance;

        for (Proxy& p : m_Proxies)
        {
            RaycastHit h = (p.collider->type == ColliderType::Box)
                               ? RayVsAABB(ray, p.bounds)
                               : RayVsSphere(ray, WorldSphere(*p.transform, *p.collider));
            if (h.hit && h.distance >= 0.0f && h.distance < best.distance)
            {
                best = h;
                best.entity = p.entity;
            }
        }

        const RaycastHit terrain = RaycastTerrain(ray, maxDistance);
        if (terrain.hit && terrain.distance < best.distance)
        {
            best = terrain;
            best.entity = ecs::kNull;
        }

        return best;
    }
}
