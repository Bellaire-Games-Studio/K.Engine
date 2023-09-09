#pragma once
#include <Event/Event.hpp>

namespace KDot
{

    class WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height) : m_Width(width), m_Height(height) {}
        inline unsigned int GetWidth() const { return m_Width; }
        inline unsigned int GetHeight() const { return m_Height; }

        std::string ToString() const override
        {
            std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
        }
        
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(WindowResize)
    private:
        unsigned int m_Width, m_Height;
    }; class WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() = default;
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(WindowClose)
    }; class WindowFocusEvent : public Event
    {
    public:
        WindowFocusEvent() = default;
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(WindowFocus)
    }; class WindowLostFocusEvent : public Event
    {
    public:
        WindowLostFocusEvent() = default;
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(WindowLostFocus)
    }; class TickEvent : public Event
    {
    public:
        TickEvent() = default;
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(Step)
    }; class UpdateEvent : public Event
    {
    public:
        UpdateEvent() = default;
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(Update)
    }; class RenderEvent : public Event
    {
    public:
        RenderEvent() = default;
        EVENT_CLASS_SOURCE(ApplicationEvent)
        EVENT_CLASS_TYPE(Render)
    }; 
    using KeyCode = uint16_t;
    namespace Key {
        enum : KeyCode {
        Backspace = 8,
        Tab = 9,
        Enter = 13,
        LeftShift = 16,
        RightShift = 16,
        LeftControl = 17,
        RightControl = 17,
        LeftAlt = 18,
        RightAlt = 18,
        Break = 19,
        CapsLock = 20,
        Escape = 27,
        PageUp = 33,
        PageDown = 34,
        End = 35,
        Home = 36,
        Left = 37,
        Up = 38,
        Right = 39,
        Down = 40,
        Insert = 45,
        Delete = 46,
        Number0 = 48,
        Number1 = 49,
        Number2 = 50,
        Number3 = 51,
        Number4 = 52,
        Number5 = 53,
        Number6 = 54,
        Number7 = 55,
        Number8 = 56,
        Number9 = 57,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        LeftWindow = 91,
        RightWindow = 92,
        Select = 93,
        Numpad0 = 96,
        Numpad1 = 97,
        Numpad2 = 98,
        Numpad3 = 99,
        Numpad4 = 100,
        Numpad5 = 101,
        Numpad6 = 102,
        Numpad7 = 103,
        Numpad = 104,
        Numpad8 = 105,
        Multiply = 106,
        Add = 107,
        Subtract = 109,
        Decimal = 110,
        Divide = 111,
        F1 = 112,
        F2 = 113,
        F3 = 114,
        F4 = 115,
        F5 = 116,
        F6 = 117,
        F7 = 118,
        F8 = 119,
        F9 = 120,
        F10 = 121,
        F11 = 122,
        F12 = 123,
        NumberLock = 144,
        ScrollLock = 145,
        Semicolon = 186,
        Equal = 187,
        Comma = 188,
        Dash = 189,
        Period = 190,
        ForwardSlash = 191,
        GraveAccent = 192,
        OpenBracket = 219,
        BackSlash = 220,
        ClosingBracket = 221,
        SingleQuote = 222
    };
    };  class KeyEvent : public Event
    {
    public:
        inline int GetKeyCode() const { return m_KeyCode; }
        EVENT_CLASS_SOURCE(KeyboardEvent)
    protected:
        KeyEvent(const KeyCode keycode) : m_KeyCode(keycode) {}
        KeyCode m_KeyCode;
    }; class KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(KeyCode keycode, int repeatCount) : KeyEvent(keycode), m_RepeatCount(repeatCount) {}
        inline int GetRepeatCount() const { return m_RepeatCount; }
        std::string ToString() const override
        {

            std::stringstream ss;
			ss << "KeyPressedEvent: " << m_KeyCode << " (repeat = " << (m_RepeatCount > 0) << ")";
			return ss.str();
        }
        EVENT_CLASS_TYPE(KeyPressed)
    private:
        int m_RepeatCount;
    }; class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(KeyCode keycode) : KeyEvent(keycode) {}
        std::string ToString() const override
        {

            
            std::stringstream ss;
			ss << "KeyReleasedEvent: " << m_KeyCode;
			return ss.str();
        }
        EVENT_CLASS_TYPE(KeyReleased)
    }; class KeyTypedEvent : public KeyEvent
    {
    public:
        KeyTypedEvent(KeyCode keycode) : KeyEvent(keycode) {}
        std::string ToString() const override
        {
            std::stringstream ss;
			ss << "KeyTypedEvent: " << m_KeyCode;
			return ss.str();
        }
        EVENT_CLASS_TYPE(KeyTyped)
    }; 
    using MouseCode = uint16_t;
    namespace Mouse {
        enum : MouseCode
        {
            Button0 = 0,
            Button1 = 1,
            Button2 = 2,
            Button3 = 3,
            Button4 = 4,

            MiddleClick = Button1,
            RightClick = Button2,
            LeftClick = Button0
        };
    };
    
    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const float x, const float y) : m_MouseX(x), m_MouseY(y) {}
        inline float GetX() const { return m_MouseX; }
        inline float GetY() const { return m_MouseY; }
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "Mouse Moved to " << m_MouseX << ", " << m_MouseY;
            return ss.str();
        }
        EVENT_CLASS_SOURCE(MouseEvent)
        EVENT_CLASS_TYPE(MouseMoved)
    private:
        float m_MouseX, m_MouseY;
    };
    class MouseScrollEvent : public Event
    {
    public:
        MouseScrollEvent(const float xOffset, const float yOffset) : m_XOffset(xOffset), m_YOffset(yOffset) {}
        inline float GetXOffset() const { return m_XOffset; }
        inline float GetYOffset() const { return m_YOffset; }
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "Mouse Scrolled to " << m_XOffset << ", " << m_YOffset;
            return ss.str();
        }
        EVENT_CLASS_SOURCE(MouseEvent)
        EVENT_CLASS_TYPE(MouseScrolled)
    private:
        float m_XOffset, m_YOffset;
        
    }; class MouseButtonEvent : public Event
    {   
    public:

        inline MouseCode GetMouseButton() const { return m_Button; }
        EVENT_CLASS_SOURCE(MouseEvent)
    protected:
        MouseButtonEvent(const MouseCode button) : m_Button(button) {}
        MouseCode m_Button;
    }; class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(const MouseCode button) : MouseButtonEvent(button) {}
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "Event: MouseButtonPressed, Button: " << m_Button;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseButtonPressed)
    }; class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(const MouseCode button) : MouseButtonEvent(button) {}
        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "Event: MouseButtonReleased, Button: " << m_Button;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

}