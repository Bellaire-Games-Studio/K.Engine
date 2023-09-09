#include <ImGuiLayer.hpp>
#include <Application.hpp>
namespace KDot
{
    ImGuiLayer::ImGuiLayer()
        : Layer("ImGuiLayer")
    {
    }
    ImGuiLayer::~ImGuiLayer()
    {
    }
    void ImGuiLayer::OnAttach()
    {
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  

        ImGui::StyleColorsDark();
        
        io.Fonts -> AddFontFromFileTTF("Assets/Fonts/PTSans-Regular.ttf", 16.0f);
        
        Application& app = Application::Get();
        ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow(), true);
        ImGui_ImplOpenGL3_Init("#version 300 es");
    }
    void ImGuiLayer::OnDetach()
    {
        ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
    }
    
    void ImGuiLayer::OnEvent(Event& event)
    {       
        if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			event.Ended |= event.IsSameSource(MouseEvent) & io.WantCaptureMouse;
			event.Ended |= event.IsSameSource(KeyboardEvent) & io.WantCaptureKeyboard;
		}
    }
    void ImGuiLayer::Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
    }
    void ImGuiLayer::End()
    {
        ImGuiIO& io = ImGui::GetIO();
        Application& app = Application::Get();
        io.DisplaySize = ImVec2((float)app.GetWindow().Width(), (float)app.GetWindow().Height());
        
        ImGui::Render();
        

    }
    void ImGuiLayer::OPGLRender()
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}