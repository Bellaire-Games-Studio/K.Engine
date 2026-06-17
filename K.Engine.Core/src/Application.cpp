#include "K.hpp"

// Web entry point used by emscripten_set_main_loop_arg; on desktop the loop in
// Application::Run() calls RunFrame() directly.
void RunApp(void* ARG)
{
    KDot::Application* app = static_cast<KDot::Application*>(ARG);
    app->RunFrame();
}

namespace KDot
{
    // Monotonic time in milliseconds, portable across web and desktop.
    static double NowMs()
    {
#if defined(KE_PLATFORM_WEB)
        return emscripten_get_now();
#else
        return glfwGetTime() * 1000.0;
#endif
    }

    Application *Application::s_Instance = nullptr;
    Application::Application(const Specification &spec)
    {
        s_Instance = this;

        m_Window = Window::Create(InitOptions(spec.Title));
        m_Window->setEventCallback(BindEvent(Application::onEvent));
        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer);
    }
    Application::~Application()
    {
        // Eventually will shutdown all the layers
    }
    void Application::PushLayer(Layer *layer)
    {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }
    void Application::PushOverlay(Layer *overlay)
    {
        m_LayerStack.PushOverlay(overlay);
        overlay->OnAttach();
    }
    void Application::onEvent(Event &e)
    {
        EventDispatcher dispatcher(e);
        if (e.GetEventType() == EventType::KeyPressed) {
        }
        dispatcher.Dispatch<WindowCloseEvent>(BindEvent(Application::OnWindowClose));
        dispatcher.Dispatch<WindowResizeEvent>(BindEvent(Application::OnWindowResize));
        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            if (e.Ended)
                break;
            (*it)->OnEvent(e);
        }
    }
    void Application::Quit()
    {
        m_Running = false;
#if defined(KE_PLATFORM_WEB)
        emscripten_cancel_main_loop();
#endif
    }
    bool Application::OnWindowClose(WindowCloseEvent &e)
    {
        Quit();
        return true;
    }
    bool Application::OnWindowResize(WindowResizeEvent &e)
    {
        if (e.GetWidth() == 0 || e.GetHeight() == 0)
        {
            return false;
        }
        return false;
    }

    void Application::RunFrame()
    {
        const double time = NowMs();
        const double timestep = time - m_LastFrameTime;
        m_LastFrameTime = time;

        Window& window = GetWindow();
        ImGuiLayer* imGuiLayer = GetImGuiLayer();

        for (Layer* layer : m_LayerStack)
            layer->Update(timestep);

        imGuiLayer->Begin();
        for (Layer* layer : m_LayerStack)
            layer->Render();
        imGuiLayer->End();

        for (Layer* layer : m_LayerStack)
            layer->PostRender();
        imGuiLayer->OPGLRender();

        window.Update();
    }

    void Application::Run()
    {
        m_LastFrameTime = NowMs();
#if defined(KE_PLATFORM_WEB)
        emscripten_set_main_loop_arg(&RunApp, this, 0, 1);
#else
        while (m_Running && !GetWindow().ShouldClose())
            RunFrame();
#endif
    }
}
