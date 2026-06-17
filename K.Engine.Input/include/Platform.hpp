#pragma once
#include <Window.hpp>
#include <Event/Events.hpp>
#include <functional>
#include <Platform/GL.hpp>

namespace KDot
{
#if defined(KE_PLATFORM_WEB)
    class JavascriptWindow : public Window // Web/WebGL backend (Emscripten)
    {
        public:
            JavascriptWindow(const InitOptions& props);
            virtual ~JavascriptWindow();
            void Update() override;
            bool ShouldClose() const override { return false; } // browser drives the loop
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
#else
    class DesktopWindow : public Window // Native Windows / Linux / macOS (GLFW + GL 3.3 core)
    {
        public:
            DesktopWindow(const InitOptions& props);
            virtual ~DesktopWindow();
            void Update() override;
            bool ShouldClose() const override;
            unsigned int Width() const override { return m_Data.Width; };
            unsigned int Height() const override { return m_Data.Height; };
            void setEventCallback(const std::function<void(Event&)>& callback) override { m_Data.EventCallback = callback; };
            virtual void* GetNativeWindow() const override { return m_Window; };
        private:
            void Init(const InitOptions& props);
            void Unload();
        private:
            GLFWwindow* m_Window = nullptr;
            struct WindowData
            {
                std::string Title;
                unsigned int Width, Height;

                std::function<void(Event&)> EventCallback;
            };
            WindowData m_Data;
    };
#endif
}
