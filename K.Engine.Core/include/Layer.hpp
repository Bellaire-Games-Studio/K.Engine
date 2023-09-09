#pragma once

#include <stdio.h>
#include <string>
#include <Event/Event.hpp>

namespace KDot
{
    class Layer
    {
        public:
            Layer(const std::string& name = "Layer");
            virtual ~Layer() = default;
            virtual void Update(const double deltaTime) {};
            virtual void Render() {};
            virtual void PostRender() {};
            virtual void OnEvent(Event& e) {};
            virtual void OnAttach() {};
            virtual void OnDetach() {};

            const std::string Name() const { return m_LayerName; }
        protected:
            std::string m_LayerName;

    };
} // namespace KDot
