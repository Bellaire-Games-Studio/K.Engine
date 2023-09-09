#pragma once


#include <Layer.hpp>
#include <Window.hpp>
#include <LayerStack.hpp>
#include <ImGuiLayer.hpp>
#include <Event/Events.hpp>

int main(); // This is the entry point for the application
void RunApp(void* ARG);
namespace KDot
{
    struct Specification
    {
        int Width;
        int Height;
        const char* Title;
    };
    class Application
    {
        public:
            Application(const Specification& spec);
            ~Application();
            void onEvent(Event& e);
            void PushLayer(Layer* layer);
            void PushOverlay(Layer* overlay);
            Window& GetWindow() { return *m_Window; }
            ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
            void Quit();

            static Application& Get() { return *s_Instance; }
            const Specification GetSpecification() const { return m_Specification; }
            float m_LastFrameTime = 0.0f;
            
        private:
            void Run();
            bool OnWindowClose(WindowCloseEvent& e);
            bool OnWindowResize(WindowResizeEvent& e); 
            Specification m_Specification;
            std::unique_ptr<Window> m_Window;
            ImGuiLayer* m_ImGuiLayer;
            bool m_Running = true;
            LayerStack m_LayerStack;
            static Application* s_Instance;
            
            friend int ::main();
            friend void ::RunApp(void* ARG);
            
    };
    Application* CreateApplication();
}
