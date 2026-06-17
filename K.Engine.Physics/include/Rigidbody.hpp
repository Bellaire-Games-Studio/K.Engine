#pragma once
#include <glm.hpp>

namespace KDot
{
    // Dynamics state for an entity. Pair with a Transform (position) and a
    // Collider component; PhysicsWorld integrates and resolves them.
    struct Rigidbody
    {
        glm::vec3 velocity{0.0f};
        glm::vec3 accumulatedForce{0.0f};

        float mass          = 1.0f;
        float invMass       = 1.0f;  // 0 => infinite mass (immovable)
        float restitution   = 0.15f; // bounciness [0,1]
        float friction      = 0.5f;  // tangential damping on contact [0,1]
        float linearDamping = 0.02f; // per-second velocity bleed
        bool  useGravity    = true;
        bool  isStatic      = false; // static bodies never move
        bool  onGround      = false; // set by the solver each step

        void SetMass(float m)
        {
            mass = m;
            invMass = (m > 0.0f && !isStatic) ? 1.0f / m : 0.0f;
        }

        void SetStatic(bool s)
        {
            isStatic = s;
            invMass = (s || mass <= 0.0f) ? 0.0f : 1.0f / mass;
        }

        void ApplyForce(const glm::vec3& f)   { accumulatedForce += f; }
        void ApplyImpulse(const glm::vec3& j) { velocity += j * invMass; }
    };

    enum class ColliderType
    {
        Box,
        Sphere
    };

    // A single collider attached to an entity. Box uses halfExtents; Sphere uses
    // radius. localOffset shifts the shape relative to the Transform position.
    struct Collider
    {
        ColliderType type = ColliderType::Box;
        glm::vec3    halfExtents{0.5f}; // Box
        float        radius = 0.5f;     // Sphere
        glm::vec3    localOffset{0.0f};
        bool         isTrigger = false; // detect overlaps but don't resolve them

        static Collider MakeBox(const glm::vec3& half, const glm::vec3& offset = glm::vec3(0.0f))
        {
            Collider c;
            c.type = ColliderType::Box;
            c.halfExtents = half;
            c.localOffset = offset;
            return c;
        }

        static Collider MakeSphere(float r, const glm::vec3& offset = glm::vec3(0.0f))
        {
            Collider c;
            c.type = ColliderType::Sphere;
            c.radius = r;
            c.localOffset = offset;
            return c;
        }
    };
}
