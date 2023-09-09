#include "K.hpp"

void RunApp(void* ARG)
    {
        KDot::Application* arg = static_cast<KDot::Application*>(ARG);
        float lastFrameTime = arg->m_LastFrameTime;
        float time = emscripten_get_now();
        float timestep = time - lastFrameTime;
        arg->m_LastFrameTime = time;
        
        KDot::Window& window = arg->GetWindow();
        KDot::ImGuiLayer* imGuiLayer = arg->GetImGuiLayer();
        for (KDot::Layer* layer : arg->m_LayerStack)
            layer->Update(timestep);
        imGuiLayer->Begin();
        for (KDot::Layer* layer : arg->m_LayerStack)
            layer->Render();
           
        imGuiLayer->End();
        for (KDot::Layer* layer : arg->m_LayerStack)
            layer->PostRender(); 
        imGuiLayer->OPGLRender();
        window.Update();
    }
namespace KDot
{
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
        emscripten_cancel_main_loop();
        
        
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

    void Application::Run()
    {
        emscripten_set_main_loop_arg(&RunApp, this, 0, 1);
        
    }
    
}

