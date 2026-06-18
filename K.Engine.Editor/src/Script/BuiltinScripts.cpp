#include <Script/ScriptBehavior.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <cmath>

// -----------------------------------------------------------------------------
// Built-in example behaviours.
//
//  These are ordinary game scripts: they derive from KDot::ScriptBehavior and
//  manipulate components through the KDot namespace. They double as the "stdlib"
//  of behaviours the editor offers in the Script component drop-down. Add your
//  own the same way and register it with KE_REGISTER_SCRIPT.
// -----------------------------------------------------------------------------
namespace KDot
{
    // Continuously yaw the entity about the Y axis.
    class SpinScript : public ScriptBehavior
    {
    public:
        void OnUpdate(float dt) override
        {
            if (Transform* t = GetTransform())
                t->rotation = glm::angleAxis(glm::radians(m_Speed * dt), glm::vec3(0, 1, 0)) * t->rotation;
        }

    private:
        float m_Speed = 90.0f; // degrees / second
    };

    // Bob up and down around the starting height.
    class HoverScript : public ScriptBehavior
    {
    public:
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
        float m_BaseY = 0.0f;
        float m_T = 0.0f;
        float m_Amplitude = 10.0f;
        float m_Frequency = 1.6f;
    };

    // Orbit the starting position in the XZ plane.
    class PatrolScript : public ScriptBehavior
    {
    public:
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
        glm::vec3 m_Center{0.0f};
        float     m_T = 0.0f;
        float     m_Radius = 40.0f;
        float     m_Speed = 0.8f; // radians / second
    };
    // Registered inside namespace KDot so the class names resolve.
    KE_REGISTER_SCRIPT(SpinScript, "Spin")
    KE_REGISTER_SCRIPT(HoverScript, "Hover")
    KE_REGISTER_SCRIPT(PatrolScript, "Patrol")
}
