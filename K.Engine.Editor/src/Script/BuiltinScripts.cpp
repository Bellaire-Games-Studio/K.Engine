#include <Script/ScriptBehavior.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <cmath>

// -----------------------------------------------------------------------------
// Built-in example behaviours.
//
//  These are ordinary game scripts: they derive from KDot::ScriptBehavior and
//  manipulate components through the KDot namespace. OnInspect() declares each
//  behaviour's authored parameters, which the editor shows + edits and the
//  serializer round-trips. They double as the "stdlib" of behaviours the editor
//  offers in the Script component drop-down.
// -----------------------------------------------------------------------------
namespace KDot
{
    // Continuously rotate the entity about a configurable axis.
    class SpinScript : public ScriptBehavior
    {
    public:
        void OnInspect(ScriptParams& p) override
        {
            p.Float("speed", m_Speed, -720.0f, 720.0f); // degrees / second
            p.Vec3("axis", m_Axis);
        }
        void OnUpdate(float dt) override
        {
            if (Transform* t = GetTransform())
            {
                const glm::vec3 axis = glm::length(m_Axis) > 1e-4f ? glm::normalize(m_Axis) : glm::vec3(0, 1, 0);
                t->rotation = glm::angleAxis(glm::radians(m_Speed * dt), axis) * t->rotation;
            }
        }

    private:
        float     m_Speed = 90.0f;
        glm::vec3 m_Axis{0.0f, 1.0f, 0.0f};
    };

    // Bob up and down around the starting height.
    class HoverScript : public ScriptBehavior
    {
    public:
        void OnInspect(ScriptParams& p) override
        {
            p.Float("amplitude", m_Amplitude, 0.0f, 100.0f);
            p.Float("frequency", m_Frequency, 0.0f, 10.0f);
        }
        void OnStart() override
        {
            if (Transform* t = GetTransform())
                m_BaseY = t->position.y;
        }
        void OnUpdate(float dt) override
        {
            m_T += dt;
            if (Transform* t = GetTransform())
                t->position.y = m_BaseY + m_Amplitude * std::sin(m_T * m_Frequency);
        }

    private:
        float m_Amplitude = 10.0f; // authored
        float m_Frequency = 1.6f;  // authored
        float m_BaseY = 0.0f;      // runtime
        float m_T = 0.0f;          // runtime
    };

    // Orbit the starting position in the XZ plane.
    class PatrolScript : public ScriptBehavior
    {
    public:
        void OnInspect(ScriptParams& p) override
        {
            p.Float("radius", m_Radius, 0.0f, 500.0f);
            p.Float("speed", m_Speed, -10.0f, 10.0f); // radians / second
        }
        void OnStart() override
        {
            if (Transform* t = GetTransform())
                m_Center = t->position;
        }
        void OnUpdate(float dt) override
        {
            m_T += dt;
            if (Transform* t = GetTransform())
            {
                t->position.x = m_Center.x + m_Radius * std::cos(m_T * m_Speed);
                t->position.z = m_Center.z + m_Radius * std::sin(m_T * m_Speed);
            }
        }

    private:
        float     m_Radius = 40.0f; // authored
        float     m_Speed = 0.8f;   // authored
        glm::vec3 m_Center{0.0f};   // runtime
        float     m_T = 0.0f;       // runtime
    };

    // Registered inside namespace KDot so the class names resolve.
    KE_REGISTER_SCRIPT(SpinScript, "Spin")
    KE_REGISTER_SCRIPT(HoverScript, "Hover")
    KE_REGISTER_SCRIPT(PatrolScript, "Patrol")
}
