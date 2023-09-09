#pragma once
#include <iostream>
#include <stdio.h>
#include <string>
#include <queue>
#include <sstream>

namespace KDot
{

    enum class EventType
    {
        Unknown = 0,
        Step,
        Update, // Called before render, but after step; used for updating game logic (Input, Physics, etc.)
        Render, // Called just before the window is rendered
        Frame, // Called after the window is rendered
        WindowClose,
        WindowInit,
        WindowResize,
        WindowDeviceMotion,
        WindowDeviceOrientation,
        WindowFocus,
        WindowLostFocus,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseMoved,
        MouseScrolled,
        MouseButtonPressed,
        MouseButtonReleased,
        JoystickConnected,
        JoystickDisconnected,
        JoystickButtonPressed,
        JoystickButtonReleased,
        JoystickMoved,
        JoystickAxisMoved,
        TouchBegan,
        TouchMoved,
        TouchEnded,
        TouchCancelled,
        SensorUpdated,

    }; 
    enum EventSource
    {
        Unknown = 0,
        ApplicationEvent = 1 << 0,
        WindowEvent = 1 << 1,
        KeyboardEvent = 1 << 2,
        MouseEvent = 1 << 3,
        MouseClickEvent = 1 << 4,
        JoystickEvent = 1 << 5,
        TouchEvent = 1 << 6,
        SensorEvent = 1 << 7,
        All = 0xFFFF
    };
    #define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; } \
                                    virtual EventType GetEventType() const override { return GetStaticType(); } \
                                    virtual const char* GetName() const override { return #type; }
    #define EVENT_CLASS_SOURCE(source) virtual int GetSourceFlags() const override { return source; }
    // Thank you Cherno

    class Event
    {
        public:
            bool Ended = false;
            virtual ~Event() = default;
            virtual EventType GetEventType() const = 0;
            virtual const char* GetName() const = 0;
            virtual int GetSourceFlags() const = 0;
            virtual std::string ToString() const { return GetName(); }
            inline bool IsSameSource(EventSource source)
            {
                return GetSourceFlags() & source;
            }
    };
    class EventDispatcher
    {
        public:
            EventDispatcher(Event& event)
                : m_Event(event)
            {
            }
            template <typename T, typename F>
            bool Dispatch(const F& func)
            {
                if (m_Event.GetEventType() == T::GetStaticType())
                {
                    m_Event.Ended |= func(static_cast<T&>(m_Event));
                    
                    return true;
                }
                return false;
            }
        private:
            Event& m_Event;
    };

} // namespace KDot
