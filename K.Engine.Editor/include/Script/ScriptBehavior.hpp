#pragma once
// -----------------------------------------------------------------------------
// K.Engine scripting
//
//  Game logic lives in C++ behaviour classes that derive from ScriptBehavior and
//  use the KDot namespace (ecs, Transform, Input, ...). A behaviour is registered
//  by name with KE_REGISTER_SCRIPT and attached to an entity through a Script
//  component; the engine instantiates it and drives its lifecycle (OnStart /
//  OnUpdate) while the scene is playing.
//
//  Because behaviours are compiled into the binary, this works identically on
//  native desktop and on the web (Emscripten/WebAssembly) - no separate scripting
//  VM or dynamic loading required. The goal is "engine for the game": the engine
//  owns the lifecycle, the game owns the behaviours.
//
//      class Spin : public KDot::ScriptBehavior {
//          void OnUpdate(float dt) override {
//              if (auto* t = GetTransform())
//                  t->rotation = glm::angleAxis(glm::radians(90.0f*dt),
//                                               glm::vec3(0,1,0)) * t->rotation;
//          }
//      };
//      KE_REGISTER_SCRIPT(Spin, "Spin");
// -----------------------------------------------------------------------------
#include <EntitityComponentSystem/ECS.hpp>
#include <Core/Transform.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace KDot
{
    // Base class for all game behaviours.
    class ScriptBehavior
    {
    public:
        virtual ~ScriptBehavior() = default;

        virtual void OnStart() {}            // called once, when play begins
        virtual void OnUpdate(float dt) {}   // called every simulated frame

        // Bound by the runtime before OnStart; gives the behaviour its context.
        void Attach(ecs::Registry* reg, ecs::Entity self)
        {
            m_Reg = reg;
            m_Self = self;
        }

    protected:
        ecs::Entity    Self() const { return m_Self; }
        ecs::Registry& Scene() const { return *m_Reg; }

        Transform* GetTransform() const { return m_Reg ? m_Reg->TryGet<Transform>(m_Self) : nullptr; }
        template <typename T> T* Get() const { return m_Reg ? m_Reg->TryGet<T>(m_Self) : nullptr; }

    private:
        ecs::Registry* m_Reg = nullptr;
        ecs::Entity    m_Self = ecs::kNull;
    };

    // Name -> factory map of every registered behaviour.
    class ScriptRegistry
    {
    public:
        using Factory = std::function<std::unique_ptr<ScriptBehavior>()>;

        static ScriptRegistry& Get();

        bool Register(const std::string& name, Factory factory);
        std::unique_ptr<ScriptBehavior> Create(const std::string& name) const;
        bool Has(const std::string& name) const;
        const std::vector<std::string>& Names() const { return m_Names; }

    private:
        std::unordered_map<std::string, Factory> m_Factories;
        std::vector<std::string>                 m_Names; // registration order, for UI
    };

    // The component that binds a behaviour to an entity. Only 'name' is
    // serialized; 'instance' is created at play time and lives only while playing.
    struct Script
    {
        std::string                     name;
        std::unique_ptr<ScriptBehavior> instance;
        bool                            started = false;
    };
}

// Registers TYPE under the string NAME at static-init time.
#define KE_REGISTER_SCRIPT(TYPE, NAME)                                              \
    namespace                                                                       \
    {                                                                               \
        const bool ke_autoreg_##TYPE = ::KDot::ScriptRegistry::Get().Register(      \
            NAME, []() { return std::unique_ptr<::KDot::ScriptBehavior>(new TYPE()); }); \
    }
