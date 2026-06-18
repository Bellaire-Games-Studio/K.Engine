#pragma once
#include <EntitityComponentSystem/ECS.hpp>
#include <glm.hpp>
#include <string>

namespace KDot
{
    // Plain environment/world settings that live outside the ECS (lighting, fog,
    // terrain seed, grass). Kept graphics-API-agnostic so the serializer has no
    // dependency on the renderer; the app maps this to/from its LightManager etc.
    struct SceneEnv
    {
        glm::vec3 sunDir{-0.40f, -0.82f, -0.45f};
        glm::vec3 sunColor{1.0f, 0.96f, 0.88f};
        float     sunIntensity = 1.15f;
        glm::vec3 ambientColor{0.55f, 0.65f, 0.85f};
        float     ambientIntensity = 0.28f;
        glm::vec3 fogColor{0.45f, 0.62f, 0.85f};
        float     fogDensity = 0.00075f;
        int       terrainSeed = 1337;
        bool      grassShow = true;
        float     grassDistance = 320.0f;
    };

    // Text (.kscene) serializer for the world: environment + every named entity
    // and its components (Transform, Prop, LightSource, Rigidbody, Collider,
    // Script). Also used in-memory to snapshot/restore state across play mode.
    namespace SceneSerializer
    {
        std::string SaveToString(ecs::Registry& reg, const SceneEnv& env);
        // Replaces the registry's contents with the parsed scene. Returns false
        // on a malformed header.
        bool        LoadFromString(const std::string& text, ecs::Registry& reg, SceneEnv& env);

        bool Save(const std::string& path, ecs::Registry& reg, const SceneEnv& env);
        bool Load(const std::string& path, ecs::Registry& reg, SceneEnv& env);
    }
}
