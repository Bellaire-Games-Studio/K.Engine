#include "K.hpp" // Main header file for K.Engine.Core, includes Application, which is the main class/wrapper for the engine, and Window, which creates a display window and handles events within JavaScript

#include "imgui.h"      // External library, C++ ui library
#include "imgui_internal.h"
#include "Renderer.hpp" // Created by me; Used to render a static triangle to a frame buffer and display within a window
using namespace std;

namespace KDot
{

    class Tetris : public Layer
    {
        Renderer m_Renderer;

    public:
        float GRAVITY = -0.075f;
        glm::vec2 m_Position = {0.0f, 0.0f};
        ImVec2 viewportSize = ImVec2(0.0f, 0.0f);

        double cumulatedTime = 0.0;
        double interval = 1000.0;
        float angle = 0.0f;
        Camera m_Camera = Camera();
        bool first = true;
        std::vector<std::vector<int>> Cells;
        bool resize = false;
        Tetris() : Layer("Test Layer")
        {
            
            m_Renderer.Compile();
            m_Renderer.GenerateFrameBuffer();

            //m_Shapes = new Shape[100];
            
        }
        ~Tetris()
        {
        }
        virtual void Update(const double deltaTime) override
        {
            // Delta time is the time between frames in milliseconds
            if (resize)
            {
                m_Renderer.RescaleFrameBuffer(viewportSize.x, viewportSize.y);
                resize = false;
            }
            cumulatedTime += deltaTime;
            if (cumulatedTime >= interval)
            {
                Interval();
                cumulatedTime = 0.0;
            }
            m_Renderer.BindFrameBuffer();
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glEnable(GL_DEPTH_TEST);  
            glEnable(GL_CULL_FACE); 
            glCullFace(GL_BACK);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            m_Camera.Update(deltaTime);
            m_Renderer.BeginStream(m_Camera);
            Draw(deltaTime);
            m_Renderer.EndStream();

            m_Renderer.UnbindFrameBuffer();
        }
        virtual void OnEvent(Event &e) override
        {
            EventDispatcher dispatcher(e);
            dispatcher.Dispatch<KeyPressedEvent>(BindEvent(Tetris::OnKeyPressed));
            // Simply runs the OnKeyPressed function if the event is a KeyPressedEvent
            //  printf("TestLayer::Event\n");
        }
        bool OnKeyPressed(KeyPressedEvent &e)
        {
            
            return false;
        }
        void Draw(const double delta) // Repeatedly streams vertex data for a cube into the renderer, takes in a delta time value to animate the cubes
        {
            angle += 0.0f;
            if (angle > 360.0f)
                angle -= 360.0f;
            for (int x = 0; x < 10; x++) {
                for (int y= 0; y < 10; y++) {
                    for (int z = 0; z < 10; z++) {
                        m_Renderer.DrawCube({ (float)x* 5 - 0.5f, (float)y * 5, (float)z * 5.0f}, {0.25f, 0.25f, 0.25f}, {0.05f, 0.5f, 0.4f, 1.0f}, angle);
                    }
                }
            }
               
            
        }
        void Interval()
        {
            
        }
        
        virtual void Render() override //Renders UI to the screen, and creates a window to display the rendered data using a frame buffer
        {
            static bool dockspace = true;
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
            if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
                window_flags |= ImGuiWindowFlags_NoBackground;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("K.Engine", nullptr, window_flags);
            ImGui::PopStyleVar();
            ImGui::PopStyleVar(2);      
            ImGuiIO &io = ImGui::GetIO();
            ImGuiStyle &style = ImGui::GetStyle();
            
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGuiID dockspace_id = ImGui::GetID("Dockspace");
                
                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
                if (first) {
                    first = false;
                    ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
                    ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
                    ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos);
                    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
                
                    auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.8f, nullptr, &dockspace_id);
                

                    ImGui::DockBuilderDockWindow("Viewport", dock_id_left);
                    ImGui::DockBuilderDockWindow("Stats", dockspace_id);
                    ImGui::DockBuilderFinish(dockspace_id);
                }
                

            }
            
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open..", "Ctrl+O"))
                    { /* Do stuff */
                    }
                    if (ImGui::MenuItem("New Project", "Ctrl+N"))
                    { /* Do stuff */
                    }
                    if (ImGui::MenuItem("Save Project", "Ctrl+S"))
                    { /* Do stuff */
                    }
                    if (ImGui::MenuItem("Save Project As..", "Ctrl+Shift+S"))
                    { /* Do stuff */
                    }
                    if (ImGui::MenuItem("Close Project", "Ctrl+W"))
                    { /* Do stuff */
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            
            ImGui::Begin("Stats");

            ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
            ImGui::Text("Frame Time: %f", ImGui::GetIO().DeltaTime);
            ImGui::Text("Mouse Position: %f, %f", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
            ImGui::Text("Draw Calls: %d", m_Renderer.DrawCallCount);
            ImGui::Text("Quads: %d", m_Renderer.QuadCount);
            ImGui::End();
            ImGui::Begin("Viewport");
            
            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
            viewportSize = viewportPanelSize;
            resize = true;
            
            ImGui::Image((void *)m_Renderer.GetFrameBufferTexture(), viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();
            
            
            ImGui::End();
        }

        virtual void PostRender() override
        {
        }
    };

    class TestApp : public Application
    {
    public:
        TestApp(const Specification &spec) : Application(spec)
        {
            PushLayer(new KDot::Tetris());
        }
        ~TestApp()
        {
        }
    };

    Application *CreateApplication()
    {
        Specification spec;
        // Only contains width, height and title of the window
        spec.Width = 1920;
        spec.Height = 1080;
        spec.Title = "K.Engine";
        return new TestApp(spec);
    }
}

int main()
{
    auto app = KDot::CreateApplication();
    app->Run(); // Custom implementation to run C++ within the browser using emscripten (compiles to webassembly/javascript)

    return 0;
}