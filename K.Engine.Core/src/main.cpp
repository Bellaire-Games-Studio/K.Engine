#include "K.hpp" // Main header file for K.Engine.Core, includes Application, which is the main class/wrapper for the engine, and Window, which creates a display window and handles events within JavaScript

#include "imgui.h"      // External library, C++ ui library
#include "imgui_internal.h"
#include "Renderer.hpp" // Renders geometry into a frame buffer shown in the viewport

#include <Terrain.hpp>
#include <PhysicsWorld.hpp>
#include <EntitityComponentSystem/ECS.hpp>
#include <Core/Transform.hpp>
#include <Core/QualitySettings.hpp>
#include <algorithm>

using namespace std;

namespace KDot
{
    // -------------------------------------------------------------------------
    // WorldLayer
    //
    //  Demo bringing the new foundation together: a procedurally generated,
    //  LOD'd terrain (driven by the fidelity dial), a physics body that falls
    //  and settles on the surface via the ECS + PhysicsWorld, and an editor
    //  panel that sculpts the terrain and scrubs fidelity at runtime.
    // -------------------------------------------------------------------------
    class WorldLayer : public Layer
    {
        Renderer m_Renderer;
        Camera   m_Camera;
        Terrain  m_Terrain;
        PhysicsWorld   m_Physics;
        ecs::Registry  m_Registry;
        ecs::Entity    m_Ball = ecs::kNull;

        ImVec2 viewportSize = ImVec2(0.0f, 0.0f);
        bool   first = true;

        // Editor state
        float m_Fidelity      = 0.65f;
        float m_BrushRadius   = 24.0f;
        float m_BrushStrength = 12.0f;
        int   m_Seed          = 1337;

    public:
        WorldLayer() : Layer("World")
        {
            m_Renderer.Compile();
            m_Renderer.GenerateFrameBuffer();

            QualitySettings::Get().SetFidelity(m_Fidelity);
            RegenerateTerrain();

            // Physics walks on the terrain heightfield.
            m_Physics.sampleTerrainHeight = [this](float x, float z) { return m_Terrain.HeightAt(x, z); };
            m_Physics.sampleTerrainNormal = [this](float x, float z) { return m_Terrain.NormalAt(x, z); };

            // Fly camera looking out over the terrain.
            m_Camera = Camera(glm::vec3(0.0f, 140.0f, 280.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -22.0f);

            SpawnBall();
        }

        ~WorldLayer() {}

        void RegenerateTerrain()
        {
            const int edge  = QualitySettings::Get().terrainChunkEdgeVerts;
            const int cells  = edge - 1;
            const float cell = 2.0f;

            TerrainConfig cfg;
            cfg.chunksX = 6;
            cfg.chunksZ = 6;
            cfg.cellSize = cell;
            cfg.heightScale = 85.0f;
            cfg.useWarp = true;
            cfg.seed = static_cast<std::uint32_t>(m_Seed);
            cfg.chunk.chunkEdge = edge;

            const float spanX = cfg.chunksX * cells * cell;
            const float spanZ = cfg.chunksZ * cells * cell;
            cfg.origin = glm::vec3(-spanX * 0.5f, 0.0f, -spanZ * 0.5f); // centre the world at the origin

            m_Terrain.Generate(cfg);
        }

        void SpawnBall()
        {
            if (m_Registry.Valid(m_Ball))
                m_Registry.Destroy(m_Ball);

            m_Ball = m_Registry.Create();
            const float gy = m_Terrain.HeightAt(0.0f, 0.0f) + 90.0f;
            m_Registry.Emplace<Transform>(m_Ball, glm::vec3(0.0f, gy, 0.0f));
            Rigidbody& rb = m_Registry.Emplace<Rigidbody>(m_Ball);
            rb.SetMass(2.0f);
            rb.restitution = 0.45f;
            rb.friction = 0.4f;
            m_Registry.Emplace<Collider>(m_Ball, Collider::MakeSphere(3.0f));
        }

        virtual void Update(const double deltaTime) override
        {
            float dt = static_cast<float>(deltaTime) / 1000.0f; // ms -> seconds
            dt = std::min(dt, 0.033f);                          // clamp huge first-frame steps

            m_Camera.Update(deltaTime);
            m_Terrain.Update(m_Camera.m_Position); // LOD selection + rebuild dirty chunks
            m_Physics.Step(m_Registry, dt);

            m_Renderer.BindFrameBuffer();
            glViewport(0, 0, 2560, 1440); // match the fixed framebuffer (ImGui resets the viewport each frame)
            glClearColor(0.45f, 0.62f, 0.85f, 1.0f); // sky
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            m_Renderer.BeginStream(m_Camera);

            for (const TerrainChunk& chunk : m_Terrain.Chunks())
                m_Renderer.DrawMesh(chunk.Mesh());

            if (Transform* t = m_Registry.TryGet<Transform>(m_Ball))
                m_Renderer.DrawCube(t->position, glm::vec3(6.0f), glm::vec4(0.9f, 0.3f, 0.2f, 1.0f), 0.0f);

            m_Renderer.EndStream();
            m_Renderer.UnbindFrameBuffer();
        }

        virtual void OnEvent(Event &e) override
        {
            EventDispatcher dispatcher(e);
            dispatcher.Dispatch<KeyPressedEvent>(BindEvent(WorldLayer::OnKeyPressed));
        }

        bool OnKeyPressed(KeyPressedEvent &e)
        {
            if (e.GetKeyCode() == KDot::Key::R)
                SpawnBall(); // R re-drops the ball
            return false;
        }

        virtual void Render() override
        {
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

            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGuiID dockspace_id = ImGui::GetID("Dockspace");
                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
                if (first)
                {
                    first = false;
                    ImGui::DockBuilderRemoveNode(dockspace_id);
                    ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
                    ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos);
                    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

                    auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.78f, nullptr, &dockspace_id);
                    ImGui::DockBuilderDockWindow("Viewport", dock_id_left);
                    ImGui::DockBuilderDockWindow("Inspector", dockspace_id);
                    ImGui::DockBuilderFinish(dockspace_id);
                }
            }

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open..", "Ctrl+O")) {}
                    if (ImGui::MenuItem("New Project", "Ctrl+N")) {}
                    if (ImGui::MenuItem("Save Project", "Ctrl+S")) {}
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            // ---- Inspector ----------------------------------------------------
            ImGui::Begin("Inspector");

            ImGui::Text("FPS: %.1f  (%.2f ms)", io.Framerate, io.DeltaTime * 1000.0f);
            ImGui::Text("Draw Calls: %d", m_Renderer.DrawCallCount);
            ImGui::Text("Triangles: %d", m_Renderer.Triangles);
            ImGui::Separator();

            QualitySettings& q = QualitySettings::Get();
            ImGui::TextUnformatted("Fidelity (Iruna 0.0  <->  Elden 1.0)");
            if (ImGui::SliderFloat("##fidelity", &m_Fidelity, 0.0f, 1.0f, "%.2f"))
                q.SetFidelity(m_Fidelity); // live: changes LOD bands / render distance immediately
            ImGui::Text("Render dist: %.0f   Chunk edge: %d", q.renderDistance, q.terrainChunkEdgeVerts);
            ImGui::Text("Max LOD: %d   Shadows: %s", q.maxLODLevels, q.shadowsEnabled ? "on" : "off");

            ImGui::Separator();
            ImGui::Text("Terrain: %zu chunks, %zu tris", m_Terrain.Chunks().size(), m_Terrain.TriangleCount());
            ImGui::InputInt("Seed", &m_Seed);
            if (ImGui::Button("Regenerate"))
                RegenerateTerrain(); // rebuilds with the new chunk density / seed

            ImGui::Separator();
            ImGui::TextUnformatted("Sculpt (at world origin)");
            ImGui::SliderFloat("Radius", &m_BrushRadius, 2.0f, 80.0f, "%.0f");
            ImGui::SliderFloat("Strength", &m_BrushStrength, 1.0f, 40.0f, "%.0f");
            const glm::vec2 brushXZ(0.0f, 0.0f);
            if (ImGui::Button("Raise"))   m_Terrain.Sculpt(brushXZ, m_BrushRadius, m_BrushStrength, SculptMode::Raise);
            ImGui::SameLine();
            if (ImGui::Button("Lower"))   m_Terrain.Sculpt(brushXZ, m_BrushRadius, m_BrushStrength, SculptMode::Lower);
            ImGui::SameLine();
            if (ImGui::Button("Smooth"))  m_Terrain.Sculpt(brushXZ, m_BrushRadius, m_BrushStrength, SculptMode::Smooth);
            ImGui::SameLine();
            if (ImGui::Button("Flatten")) m_Terrain.Sculpt(brushXZ, m_BrushRadius, m_BrushStrength, SculptMode::Flatten);

            ImGui::Separator();
            if (ImGui::Button("Drop ball (R)"))
                SpawnBall();
            if (Transform* t = m_Registry.TryGet<Transform>(m_Ball))
            {
                Rigidbody* rb = m_Registry.TryGet<Rigidbody>(m_Ball);
                ImGui::Text("Ball y: %.1f  grounded: %s", t->position.y, (rb && rb->onGround) ? "yes" : "no");
            }
            ImGui::TextDisabled("WASD to fly the camera");
            ImGui::End();

            // ---- Viewport -----------------------------------------------------
            ImGui::Begin("Viewport");
            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
            viewportSize = viewportPanelSize;
            ImGui::Image((void *)m_Renderer.GetFrameBufferTexture(), viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
            ImGui::End();

            ImGui::End();
        }

        virtual void PostRender() override {}
    };

    class TestApp : public Application
    {
    public:
        TestApp(const Specification &spec) : Application(spec)
        {
            PushLayer(new KDot::WorldLayer());
        }
        ~TestApp() {}
    };

    Application *CreateApplication()
    {
        Specification spec;
        spec.Width = 1920;
        spec.Height = 1080;
        spec.Title = "K.Engine";
        return new TestApp(spec);
    }
}

int main()
{
    auto app = KDot::CreateApplication();
    app->Run(); // emscripten main loop (compiles to WebAssembly via Emscripten)

    return 0;
}
