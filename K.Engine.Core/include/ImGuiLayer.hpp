#pragma once
#include <Event/Event.hpp>
#include <Layer.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace KDot
{
    class ImGuiLayer: public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnEvent(Event& e) override;
        void Begin();
        void End();
        void OPGLRender();
        void Block(bool block) { m_BlockEvents = block; }
    private:
        bool m_BlockEvents = false;
        
    };
}
