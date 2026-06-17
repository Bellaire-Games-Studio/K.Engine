#include <Platform.hpp>
#if defined(KE_PLATFORM_DESKTOP)
#include <iostream>

namespace KDot
{
    DesktopWindow::DesktopWindow(const InitOptions& options)
    {
        Init(options);
    }
    DesktopWindow::~DesktopWindow()
    {
        Unload();
    }
    void DesktopWindow::Unload()
    {
        if (m_Window)
            glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
    bool DesktopWindow::ShouldClose() const
    {
        return m_Window ? glfwWindowShouldClose(m_Window) != 0 : true;
    }
    void DesktopWindow::Update()
    {
        glfwPollEvents();
        glfwSwapBuffers(m_Window);
    }
    void DesktopWindow::Init(const InitOptions& props)
    {
        m_Data.Title = props.name;
        m_Data.Width = props.width;
        m_Data.Height = props.height;

        if (!glfwInit())
        {
            std::cerr << "DesktopWindow: glfwInit failed" << std::endl;
            return;
        }

        // Request an OpenGL 3.3 core context (matches the GLES3 feature set the
        // renderer targets; shaders are adapted from "300 es" to "330 core").
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(KE_PLATFORM_MACOS)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        GLFWwindow* window = glfwCreateWindow((int)props.width, (int)props.height, m_Data.Title.c_str(), NULL, NULL);
        if (window == nullptr)
        {
            std::cerr << "DesktopWindow: failed to create window" << std::endl;
            glfwTerminate();
            return;
        }
        m_Window = window;
        glfwMakeContextCurrent(window);

        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        if (err != GLEW_OK)
            std::cerr << "DesktopWindow: glewInit failed: " << glewGetErrorString(err) << std::endl;

        glfwSwapInterval(1); // vsync
        glfwSetWindowUserPointer(window, &m_Data);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            data.Width = width;
            data.Height = height;
            WindowResizeEvent event(width, height);
            if (data.EventCallback) data.EventCallback(event);
        });
        glfwSetWindowCloseCallback(window, [](GLFWwindow* w)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            WindowCloseEvent event;
            if (data.EventCallback) data.EventCallback(event);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            switch (action)
            {
                case GLFW_PRESS:   { KeyPressedEvent  event(key, 0); if (data.EventCallback) data.EventCallback(event); break; }
                case GLFW_RELEASE: { KeyReleasedEvent event(key);    if (data.EventCallback) data.EventCallback(event); break; }
                case GLFW_REPEAT:  { KeyPressedEvent  event(key, 1); if (data.EventCallback) data.EventCallback(event); break; }
            }
        });
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int keycode)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            KeyTypedEvent event(keycode);
            if (data.EventCallback) data.EventCallback(event);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            switch (action)
            {
                case GLFW_PRESS:   { MouseButtonPressedEvent  event(button); if (data.EventCallback) data.EventCallback(event); break; }
                case GLFW_RELEASE: { MouseButtonReleasedEvent event(button); if (data.EventCallback) data.EventCallback(event); break; }
            }
        });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double xOffset, double yOffset)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            MouseScrollEvent event((float)xOffset, (float)yOffset);
            if (data.EventCallback) data.EventCallback(event);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xPos, double yPos)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(w);
            MouseMovedEvent event((float)xPos, (float)yPos);
            if (data.EventCallback) data.EventCallback(event);
        });
    }
}
#endif // KE_PLATFORM_DESKTOP
