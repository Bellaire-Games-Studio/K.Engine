#pragma once
#include <EntitityComponentSystem/ECS.hpp>
#include <Core/Transform.hpp>
#include <Core/SceneComponents.hpp>
#include <glm.hpp>
#include <gtc/matrix_inverse.hpp>

namespace KDot
{
    // -------------------------------------------------------------------------
    // Scene-graph helpers
    //
    //  Entities form a tree via the Parent component. A Transform is *local*
    //  (relative to its parent); the world transform is the chain of locals from
    //  the root down. Game scripts read/write the local Transform directly (easy
    //  relative motion), and use WorldMatrix/WorldPosition when they need global
    //  space. A depth guard keeps a malformed (cyclic) chain from recursing
    //  forever.
    // -------------------------------------------------------------------------

    inline glm::mat4 WorldMatrix(ecs::Registry& reg, ecs::Entity e, int depth = 0)
    {
        const Transform* t = reg.TryGet<Transform>(e);
        const glm::mat4 local = t ? t->Matrix() : glm::mat4(1.0f);

        if (depth < 64)
            if (const Parent* p = reg.TryGet<Parent>(e))
                if (p->value != ecs::kNull && p->value != e && reg.Valid(p->value))
                    return WorldMatrix(reg, p->value, depth + 1) * local;

        return local;
    }

    inline glm::vec3 WorldPosition(ecs::Registry& reg, ecs::Entity e)
    {
        return glm::vec3(WorldMatrix(reg, e)[3]);
    }

    // True if 'ancestor' is e or sits above e in the tree (used to reject
    // re-parenting an entity under one of its own descendants).
    inline bool IsAncestor(ecs::Registry& reg, ecs::Entity ancestor, ecs::Entity e, int depth = 0)
    {
        if (ancestor == e)
            return true;
        if (depth >= 64)
            return false;
        if (const Parent* p = reg.TryGet<Parent>(e))
            if (p->value != ecs::kNull && reg.Valid(p->value))
                return IsAncestor(reg, ancestor, p->value, depth + 1);
        return false;
    }

    // Re-parent 'e' under 'newParent' (kNull = detach) while keeping its current
    // world transform, by recomputing its local Transform. No-op if it would
    // create a cycle.
    inline void SetParentKeepWorld(ecs::Registry& reg, ecs::Entity e, ecs::Entity newParent)
    {
        if (e == ecs::kNull || !reg.Valid(e))
            return;
        if (newParent != ecs::kNull && (!reg.Valid(newParent) || IsAncestor(reg, e, newParent)))
            return; // cycle / invalid

        Transform* t = reg.TryGet<Transform>(e);
        const glm::mat4 world = WorldMatrix(reg, e); // capture before re-parenting

        // Bake a (local) matrix back into a Transform's TRS.
        auto bake = [](Transform& tr, const glm::mat4& m) {
            tr.position = glm::vec3(m[3]);
            tr.scale = glm::vec3(glm::length(glm::vec3(m[0])),
                                 glm::length(glm::vec3(m[1])),
                                 glm::length(glm::vec3(m[2])));
            glm::mat3 rot(glm::vec3(m[0]) / glm::max(tr.scale.x, 1e-6f),
                          glm::vec3(m[1]) / glm::max(tr.scale.y, 1e-6f),
                          glm::vec3(m[2]) / glm::max(tr.scale.z, 1e-6f));
            tr.rotation = glm::quat_cast(rot);
        };

        if (newParent == ecs::kNull)
        {
            reg.Remove<Parent>(e);
            if (t)
                bake(*t, world); // detached: local becomes world
        }
        else
        {
            reg.Emplace<Parent>(e).value = newParent;
            if (t)
                bake(*t, glm::affineInverse(WorldMatrix(reg, newParent)) * world);
        }
    }
}
