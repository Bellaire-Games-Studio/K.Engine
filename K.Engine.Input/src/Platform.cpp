#include <Platform.hpp>
namespace KDot
{
    EM_JS(int, getHeight, (), {
        return window.innerHeight;
    }); // Macro to get the height of the window
    EM_JS(int, getWidth, (), {
        return window.innerWidth;
    }); // Macro to get the width of the window
    EM_JS(void, resizeCanvas, (), {
        document.getElementById("canvas").width = window.innerWidth;
        document.getElementById("canvas").height = window.innerHeight;
    });
    JavascriptWindow::JavascriptWindow(const InitOptions& options)
    {
        Init(options);
    }
    JavascriptWindow::~JavascriptWindow()
    {
        Unload();
    }
    void JavascriptWindow::Unload()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
    void JavascriptWindow::Update()
    {
        int width, height;

        width = KDot::getWidth(); 
        height = KDot::getHeight();
        if (width != m_Data.Width || height != m_Data.Height)
        {
            // Document was resized
            m_Data.Width = width;
            m_Data.Height = height;
            KDot::resizeCanvas();
            glfwSetWindowSize(m_Window, width, height); 
            
        }
        glfwPollEvents();
        glfwSwapBuffers(m_Window);
        
    }
    void JavascriptWindow::Init(const InitOptions& props)
    {
        m_Data.Title = props.name;
        m_Data.Width = props.width;
        m_Data.Height = props.height;
        if (!glfwInit())
        {
            return;
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

        GLFWwindow* window = glfwCreateWindow((int)props.width, (int)props.height, m_Data.Title.c_str(), NULL, NULL);
        if (window == nullptr)
        {
            glfwTerminate();
            return;
        }
        m_Window = window;
        glfwMakeContextCurrent(window);

        glfwSetWindowUserPointer(window, &m_Data);
        

        glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;
            
            WindowResizeEvent event(width, height);
            data.EventCallback(event);
        });
        glfwSetWindowCloseCallback(window, [](GLFWwindow* window)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            data.EventCallback(event);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            
            switch (action)
            {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
            }
        });
        glfwSetCharCallback(window, [](GLFWwindow* window, unsigned int keycode)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            KeyTypedEvent event(keycode);
            data.EventCallback(event);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            switch (action)
            {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
            }
        });
        glfwSetScrollCallback(window, [](GLFWwindow* window, double xOffset, double yOffset)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseScrollEvent event((float)xOffset, (float)yOffset);
            data.EventCallback(event);
        });
        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
		});
    }
}