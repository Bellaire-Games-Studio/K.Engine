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
#include <Core/Hierarchy.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace KDot
{
    // Visitor used to declare a behaviour's authored parameters. The same
    // OnInspect() pass drives the inspector UI, save, load, and copy - each is a
    // ScriptParams subclass that reads or writes the bound member. min<max gives
    // the inspector a slider range (otherwise it uses a drag field).
    class ScriptParams
    {
    public:
        virtual ~ScriptParams() = default;
        virtual void Float(const char* name, float& v, float min = 0.0f, float max = 0.0f) {}
        virtual void Int(const char* name, int& v) {}
        virtual void Bool(const char* name, bool& v) {}
        virtual void Vec3(const char* name, glm::vec3& v) {}
    };

    // Base class for all game behaviours.
    class ScriptBehavior
    {
    public:
        virtual ~ScriptBehavior() = default;

        virtual void OnStart() {}            // called once, when play begins
        virtual void OnUpdate(float dt) {}   // called every simulated frame

        // Declare the behaviour's editable/serialized parameters (see ScriptParams).
        virtual void OnInspect(ScriptParams&) {}

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

        // Local Transform is relative to the parent; these resolve global space.
        glm::vec3 WorldPosition() const { return m_Reg ? KDot::WorldPosition(*m_Reg, m_Self) : glm::vec3(0.0f); }
        glm::mat4 WorldMatrix()   const { return m_Reg ? KDot::WorldMatrix(*m_Reg, m_Self) : glm::mat4(1.0f); }

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
