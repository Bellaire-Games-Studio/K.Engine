#include <Script/ScriptBehavior.hpp>

namespace KDot
{
    ScriptRegistry& ScriptRegistry::Get()
    {
        static ScriptRegistry instance; // function-local static: safe init order
        return instance;
    }

    bool ScriptRegistry::Register(const std::string& name, Factory factory)
    {
        if (m_Factories.find(name) == m_Factories.end())
            m_Names.push_back(name);
        m_Factories[name] = std::move(factory);
        return true;
    }

    std::unique_ptr<ScriptBehavior> ScriptRegistry::Create(const std::string& name) const
    {
        auto it = m_Factories.find(name);
        return it == m_Factories.end() ? nullptr : it->second();
    }

    bool ScriptRegistry::Has(const std::string& name) const
    {
        return m_Factories.find(name) != m_Factories.end();
    }
}
