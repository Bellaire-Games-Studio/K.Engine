#include <Script/ScriptBehavior.hpp>
#include <cmath>

// -----------------------------------------------------------------------------
// Example user script.
//
//  This is a normal C++ behaviour authored outside the engine modules: drop a
//  .cpp in Scripts/, derive from KDot::ScriptBehavior, register it by name with
//  KE_REGISTER_SCRIPT, and rebuild. It then appears in every entity's Script
//  component drop-down. The same source compiles into both the native and the
//  web build (no separate scripting VM).
//
//  Pulse scales its entity up and down with a sine wave. Edit it, or copy it as
//  a starting point, from the in-editor "C++ Script Editor" (Scripts menu).
// -----------------------------------------------------------------------------
namespace KDot
{
    class Pulse : public ScriptBehavior
    {
    public:
        void OnInspect(ScriptParams& p) override
        {
            p.Float("amount", m_Amount, 0.0f, 1.0f);    // fraction of size to breathe
            p.Float("frequency", m_Frequency, 0.0f, 8.0f);
        }

        void OnStart() override
        {
            if (Transform* t = GetTransform())
                m_BaseScale = t->scale;
        }

        void OnUpdate(float dt) override
        {
            m_T += dt;
            if (Transform* t = GetTransform())
            {
                const float s = 1.0f + m_Amount * std::sin(m_T * m_Frequency);
                t->scale = m_BaseScale * s;
            }
        }

    private:
        float     m_Amount = 0.25f;   // authored
        float     m_Frequency = 2.0f; // authored
        glm::vec3 m_BaseScale{1.0f};  // runtime
        float     m_T = 0.0f;         // runtime
    };

    KE_REGISTER_SCRIPT(Pulse, "Pulse")
}
