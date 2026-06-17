#pragma once
#include <Shapes.hpp>
#include <Collision.hpp>
#include <Rigidbody.hpp>
#include <Core/Transform.hpp>
#include <EntitityComponentSystem/ECS.hpp>

#include <functional>
#include <vector>
#include <cstdint>

namespace KDot
{
    // -------------------------------------------------------------------------
    // PhysicsWorld
    //
    //  Steps every entity carrying Transform + Rigidbody + Collider components:
    //    1. integrate forces (semi-implicit Euler)
    //    2. resolve against the terrain heightfield (optional callback)
    //    3. spatial-hash broadphase -> narrow-phase manifolds
    //    4. impulse resolution + Baumgarte positional correction
    //
    //  It also answers world raycasts (colliders + terrain) for gameplay, AI,
    //  and editor picking.
    // -------------------------------------------------------------------------
    class PhysicsWorld
    {
    public:
        glm::vec3 gravity{0.0f, -9.81f, 0.0f};
        int       solverIterations = 4;
        float     broadphaseCellSize = 4.0f;
        float     positionalCorrection = 0.2f; // Baumgarte factor [0,1]
        float     penetrationSlop = 0.01f;

        // Optional terrain hooks. When set, bodies collide with the heightfield.
        std::function<float(float, float)>     sampleTerrainHeight;
        std::function<glm::vec3(float, float)> sampleTerrainNormal;

        void Step(ecs::Registry& registry, float dt);

        // Nearest hit against all colliders (and terrain if a height hook is set).
        RaycastHit Raycast(ecs::Registry& registry, const Ray& ray, float maxDistance = 1.0e30f);

    private:
        struct Proxy
        {
            ecs::Entity entity;
            Transform*  transform;
            Rigidbody*  body;
            Collider*   collider;
            AABB        bounds;
        };

        std::vector<Proxy> m_Proxies;

        AABB   WorldAABB(const Transform& t, const Collider& c) const;
        Sphere WorldSphere(const Transform& t, const Collider& c) const;
        Manifold ComputeManifold(const Proxy& a, const Proxy& b) const;

        void Integrate(float dt);
        void ResolveTerrain();
        void ResolveCollisions();
        RaycastHit RaycastTerrain(const Ray& ray, float maxDistance) const;
    };
}
