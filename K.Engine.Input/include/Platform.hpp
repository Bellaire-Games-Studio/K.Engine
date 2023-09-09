#pragma once
#include <Window.hpp>
#include <Event/Events.hpp>
#include <functional>
#ifdef __EMSCRIPTEN__

    #include <emscripten.h>
    #include <emscripten/html5.h>
    #include <GLFW/glfw3.h>
#endif

namespace KDot
{   

    
    class JavascriptWindow : public Window // Only platform supported for now
    {
        public:
            JavascriptWindow(const InitOptions& props);
            virtual ~JavascriptWindow();
            void Update() override;
            unsigned int Width() const override { return m_Data.Width; };
            unsigned int Height() const override { return m_Data.Height; };
            void setEventCallback(const std::function<void(Event&)>& callback) override { m_Data.EventCallback = callback; };
            virtual void* GetNativeWindow() const override { return m_Window; };
        private:
            virtual void Init(const InitOptions& props);
            virtual void Unload();
        private:
            GLFWwindow* m_Window;
            struct WindowData
            {
                std::string Title;
                unsigned int Width, Height;
                
                std::function<void(Event&)> EventCallback;
            };
            WindowData m_Data;
    };
}