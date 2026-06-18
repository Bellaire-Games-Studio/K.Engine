#pragma once
#include <string>
#include <glm.hpp>
#include <EntitityComponentSystem/ECS.hpp>

namespace KDot
{
    // Parent link: this entity's Transform is interpreted relative to the
    // parent's world transform (see Core/Hierarchy.hpp). Detaching = kNull.
    struct Parent
    {
        ecs::Entity value = ecs::kNull;
    };

    // -------------------------------------------------------------------------
    // Scene components
    //
    //  Lightweight, editor-facing components that turn ECS entities into
    //  inspectable "world items". Paired with Transform (and optionally
    //  Rigidbody / Collider), these are what the World Explorer lists and the
    //  Properties panel edits.
    // -------------------------------------------------------------------------

    // Human-readable label shown in the explorer / properties title.
    struct Name
    {
        std::string value;
    };

    // A renderable axis-aligned box prop (drawn via Renderer::DrawCube).
    struct Prop
    {
        glm::vec3 size{4.0f};
        glm::vec4 color{0.80f, 0.80f, 0.85f, 1.0f};
    };

    // A point-light emitter. Its world position comes from the entity Transform;
    // the renderer gathers these into the LightManager each frame.
    struct LightSource
    {
        glm::vec3 color{1.0f, 0.85f, 0.6f};
        float     intensity = 2.0f;
        float     radius     = 140.0f;
    };
}
