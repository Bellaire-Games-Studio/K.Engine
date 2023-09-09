
#pragma once
#include <stdio.h>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>
#include <functional>
#include <Event/Event.hpp>
#include <Scopes.hpp>
namespace KDot
{
    struct InitOptions
    {
        std::string name;
        uint32_t width;
        uint32_t height;
        

        InitOptions(const std::string& name = "K.Engine", uint32_t width = 1280, uint32_t height = 720)
            : name(name), width(width), height(height)
        {
        }
    };

    class Window
    {
        public:
            
            virtual ~Window() = default;

            virtual void Update() = 0;
            
            virtual uint32_t Width() const { return m_Width; }
            virtual uint32_t Height() const { return m_Height; }

            virtual void setEventCallback(const std::function<void(Event&)>& callback) = 0;
            virtual void* GetNativeWindow() const = 0;
            static Scope<Window> Create(const InitOptions& options = InitOptions());
        protected:
            
            uint32_t m_Width;
            uint32_t m_Height;
    };
} // namespace Kdot
