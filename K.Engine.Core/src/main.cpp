#include "K.hpp" // Main header file for K.Engine.Core

#include "imgui.h"
#include "imgui_internal.h"
#include "Renderer.hpp"
#include "GrassRenderer.hpp"

#include <Terrain.hpp>
#include <GrassField.hpp>
#include <PhysicsWorld.hpp>
#include <Light.hpp>
#include <Input.hpp>
#include <EntitityComponentSystem/ECS.hpp>
#include <Core/Transform.hpp>
#include <Core/QualitySettings.hpp>
#include <Core/Picking.hpp>
#include <algorithm>

using namespace std;

namespace KDot
{
    // -------------------------------------------------------------------------
    // WorldLayer
    //
    //  Demo: procedurally generated LOD terrain with GPU procedural texturing,
    //  GPU-instanced wind-animated grass, sun + point lights, a physics body
    //  that settles on the surface, mouse-driven camera, and click-to-sculpt.
    //
    //  Controls: WASD fly · right-drag look · scroll zoom ·
    //            left-click sculpt (hold Shift to lower) · R re-drop the ball
    // -------------------------------------------------------------------------
    class WorldLayer : public Layer
    {
        Renderer      m_Renderer;
        GrassRenderer m_Grass;
        Camera        m_Camera;
        Terrain       m_Terrain;
        GrassField    m_GrassField;
        PhysicsWorld  m_Physics;
        LightManager  m_Lights;
        ecs::Registry m_Registry;
        ecs::Entity   m_Ball = ecs::kNull;

        bool  first = true;
        bool  m_GrassReady = false;
        bool  m_ShowGrass = true;
        float m_Time = 0.0f;

        // Editor / interaction state
        float m_Fidelity      = 0.65f;
        float m_BrushRadius   = 28.0f;
        float m_BrushStrength = 18.0f;
        int   m_Seed          = 1337;

        glm::vec2 m_LastMouse{0.0f, 0.0f};
        bool      m_Looking = false;
        bool      m_ViewportHovered = false;
        glm::vec2 m_ViewportNDC{0.0f, 0.0f};

        static constexpr float kFbW = 2560.0f;
        static constexpr float kFbH = 1440.0f;

    public:
        WorldLayer() : Layer("World")
        {
            m_Renderer.Compile();
            m_Renderer.GenerateFrameBuffer();

            QualitySettings::Get().SetFidelity(m_Fidelity);
            m_GrassReady = m_Grass.Init();
            RegenerateTerrain(); // also scatters grass

            m_Physics.sampleTerrainHeight = [this](float x, float z) { return m_Terrain.HeightAt(x, z); };
            m_Physics.sampleTerrainNormal = [this](float x, float z) { return m_Terrain.NormalAt(x, z); };

            m_Camera = Camera(glm::vec3(0.0f, 140.0f, 280.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -22.0f);
            m_Camera.SetMovementSpeed(0.12f);

            m_Lights.ambient.color = glm::vec3(0.55f, 0.65f, 0.85f);
            m_Lights.ambient.intensity = 0.28f;
            m_Lights.sun.direction = glm::normalize(glm::vec3(-0.40f, -0.82f, -0.45f));
            m_Lights.sun.color = glm::vec3(1.0f, 0.96f, 0.88f);
            m_Lights.sun.intensity = 1.15f;
            m_Lights.fogColor = glm::vec3(0.45f, 0.62f, 0.85f);
            m_Lights.fogDensity = 0.00075f;
            m_Lights.points.push_back({glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(1.0f, 0.6f, 0.3f), 2.2f, 140.0f});
            m_Lights.points.push_back({glm::vec3(160.0f, 50.0f, -160.0f), glm::vec3(0.3f, 0.7f, 1.0f), 2.0f, 220.0f});

            SpawnBall();
        }

        ~WorldLayer() {}

        glm::mat4 Projection()
        {
            return glm::perspective(glm::radians(m_Camera.getZoom()), kFbW / kFbH, 0.1f,
                                    QualitySettings::Get().renderDistance);
        }

        void RegenerateTerrain()
        {
            const int edge = QualitySettings::Get().terrainChunkEdgeVerts;
            const int cells = edge - 1;
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
            cfg.origin = glm::vec3(-spanX * 0.5f, 0.0f, -spanZ * 0.5f);

            m_Terrain.Generate(cfg);
            RegenerateGrass();
        }

        void RegenerateGrass()
        {
            const float fid = QualitySettings::Get().Fidelity();

            GrassParams gp;
            gp.density   = 0.05f + fid * 0.45f;                       // 0.05 .. 0.5 blades / m^2
            gp.maxBlades = static_cast<int>(8000.0f + fid * 112000.0f); // 8k .. 120k
            gp.minHeight = 1.5f;
            gp.maxHeight = 62.0f;
            gp.slopeThreshold = 0.74f;
            gp.seed = static_cast<std::uint32_t>(m_Seed);

            m_GrassField.Generate(m_Terrain.Field(), gp);
            m_Grass.SetInstances(m_GrassField.Instances());
            m_Grass.maxDistance = 120.0f + fid * 380.0f; // grass view distance scales with fidelity
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

        bool PickWorld(RaycastHit& outHit)
        {
            const Picking::PickRay pr = Picking::ScreenToRay(m_ViewportNDC, m_Camera.GetViewMatrix(), Projection());
            Ray ray{pr.origin, pr.direction};
            outHit = m_Physics.Raycast(m_Registry, ray, 6000.0f);
            return outHit.hit;
        }

        virtual void Update(const double deltaTime) override
        {
            float dt = static_cast<float>(deltaTime) / 1000.0f; // ms -> seconds
            dt = std::min(dt, 0.033f);
            m_Time += dt;

            // --- Mouse look (right-drag) ---
            auto mp = Input::GetMousePosition();
            glm::vec2 mouse(mp.first, mp.second);
            const bool rightDown = Input::IsMouseButtonPressed(KDot::Mouse::RightClick);
            if (rightDown && m_Looking)
            {
                const glm::vec2 d = mouse - m_LastMouse;
                m_Camera.ProcessMouse(d.x, d.y);
            }
            m_Looking = rightDown;
            m_LastMouse = mouse;

            m_Camera.Update(deltaTime);

            // --- Click to sculpt at the picked terrain point ---
            if (m_ViewportHovered && Input::IsMouseButtonPressed(KDot::Mouse::LeftClick))
            {
                RaycastHit hit;
                if (PickWorld(hit))
                {
                    const SculptMode mode = Input::IsKeyPressed(KDot::Key::LeftShift) ? SculptMode::Lower : SculptMode::Raise;
                    m_Terrain.Sculpt(glm::vec2(hit.point.x, hit.point.z), m_BrushRadius, m_BrushStrength, mode, dt);
                }
            }

            if (!m_Lights.points.empty())
            {
                if (Transform* t = m_Registry.TryGet<Transform>(m_Ball))
                    m_Lights.points[0].position = t->position + glm::vec3(0.0f, 12.0f, 0.0f);
            }

            m_Terrain.Update(m_Camera.m_Position);
            m_Physics.Step(m_Registry, dt);

            // --- Render the scene into the offscreen framebuffer ---
            m_Renderer.BindFrameBuffer();
            glViewport(0, 0, (int)kFbW, (int)kFbH);
            glClearColor(0.45f, 0.62f, 0.85f, 1.0f);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            m_Renderer.BeginStream(m_Camera);
            m_Renderer.SetLights(m_Lights, m_Camera.m_Position);

            for (const TerrainChunk& chunk : m_Terrain.Chunks())
                m_Renderer.DrawMesh(chunk.Mesh());

            if (Transform* t = m_Registry.TryGet<Transform>(m_Ball))
                m_Renderer.DrawCube(t->position, glm::vec3(6.0f), glm::vec4(0.9f, 0.3f, 0.2f, 1.0f), 0.0f);

            m_Renderer.EndStream();

            // GPU-instanced grass (single draw call), depth-tested against terrain.
            if (m_ShowGrass && m_GrassReady)
                m_Grass.Render(m_Camera.GetViewMatrix(), Projection(), m_Camera.m_Position, m_Time, m_Lights);

            DrawHUD();

            m_Renderer.UnbindFrameBuffer();
        }

        void DrawHUD()
        {
            m_Renderer.Begin2D(kFbW, kFbH);

            const glm::vec4 white(1.0f, 1.0f, 1.0f, 0.85f);
            m_Renderer.DrawQuad(glm::vec3(kFbW * 0.5f, kFbH * 0.5f, 0.0f), glm::vec2(44.0f, 4.0f), white);
            m_Renderer.DrawQuad(glm::vec3(kFbW * 0.5f, kFbH * 0.5f, 0.0f), glm::vec2(4.0f, 44.0f), white);

            m_Renderer.DrawQuad(glm::vec3(180.0f, 90.0f, 0.0f),  glm::vec2(320.0f, 40.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
            m_Renderer.DrawQuad(glm::vec3(180.0f, 90.0f, 0.0f),  glm::vec2(300.0f, 22.0f), glm::vec4(0.85f, 0.25f, 0.22f, 0.9f));
            m_Renderer.DrawQuad(glm::vec3(180.0f, 140.0f, 0.0f), glm::vec2(320.0f, 40.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
            m_Renderer.DrawQuad(glm::vec3(180.0f, 140.0f, 0.0f), glm::vec2(220.0f, 22.0f), glm::vec4(0.25f, 0.55f, 0.95f, 0.9f));

            if (m_ViewportHovered)
            {
                const float px = (m_ViewportNDC.x * 0.5f + 0.5f) * kFbW;
                const float py = (1.0f - (m_ViewportNDC.y * 0.5f + 0.5f)) * kFbH;
                m_Renderer.DrawQuad(glm::vec3(px, py, 0.0f), glm::vec2(26.0f, 26.0f), glm::vec4(1.0f, 0.9f, 0.2f, 0.5f));
            }

            m_Renderer.End2D();
        }

        virtual void OnEvent(Event &e) override
        {
            EventDispatcher dispatcher(e);
            dispatcher.Dispatch<KeyPressedEvent>(BindEvent(WorldLayer::OnKeyPressed));
            dispatcher.Dispatch<MouseScrollEvent>(BindEvent(WorldLayer::OnMouseScroll));
        }

        bool OnKeyPressed(KeyPressedEvent &e)
        {
            if (e.GetKeyCode() == KDot::Key::R)
                SpawnBall();
            return false;
        }

        bool OnMouseScroll(MouseScrollEvent &e)
        {
            m_Camera.ProcessScroll(e.GetYOffset());
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
            ImGui::Text("Draw Calls: %d   Triangles: %d", m_Renderer.DrawCallCount, m_Renderer.Triangles);
            ImGui::Separator();

            QualitySettings& q = QualitySettings::Get();
            ImGui::TextUnformatted("Fidelity (Iruna 0.0  <->  Elden 1.0)");
            if (ImGui::SliderFloat("##fidelity", &m_Fidelity, 0.0f, 1.0f, "%.2f"))
                q.SetFidelity(m_Fidelity);
            ImGui::Text("Render dist: %.0f   Chunk edge: %d", q.renderDistance, q.terrainChunkEdgeVerts);
            ImGui::Text("Max LOD: %d   Max lights: %d", q.maxLODLevels, q.maxLights);

            ImGui::Separator();
            ImGui::Text("Terrain: %zu chunks, %zu tris", m_Terrain.Chunks().size(), m_Terrain.TriangleCount());
            ImGui::InputInt("Seed", &m_Seed);
            if (ImGui::Button("Regenerate"))
                RegenerateTerrain();

            ImGui::Separator();
            ImGui::Checkbox("Grass", &m_ShowGrass);
            ImGui::SameLine();
            ImGui::Text("%d blades (1 draw call)", m_Grass.InstanceCount());

            ImGui::Separator();
            ImGui::TextUnformatted("Lighting");
            ImGui::SliderFloat("Sun", &m_Lights.sun.intensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Ambient", &m_Lights.ambient.intensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Fog", &m_Lights.fogDensity, 0.0f, 0.003f, "%.4f");

            ImGui::Separator();
            ImGui::TextUnformatted("Brush");
            ImGui::SliderFloat("Radius", &m_BrushRadius, 4.0f, 100.0f, "%.0f");
            ImGui::SliderFloat("Strength", &m_BrushStrength, 1.0f, 60.0f, "%.0f");

            ImGui::Separator();
            if (ImGui::Button("Drop ball (R)"))
                SpawnBall();
            if (Transform* t = m_Registry.TryGet<Transform>(m_Ball))
            {
                Rigidbody* rb = m_Registry.TryGet<Rigidbody>(m_Ball);
                ImGui::Text("Ball y: %.1f  grounded: %s", t->position.y, (rb && rb->onGround) ? "yes" : "no");
            }
            ImGui::TextDisabled("WASD fly | right-drag look | scroll zoom");
            ImGui::TextDisabled("left-click sculpt (Shift = lower)");
            ImGui::End();

            // ---- Viewport (also captures cursor state for picking) ------------
            ImGui::Begin("Viewport");
            ImVec2 panelSize = ImGui::GetContentRegionAvail();
            ImGui::Image((void *)m_Renderer.GetFrameBufferTexture(), panelSize, ImVec2(0, 1), ImVec2(1, 0));

            m_ViewportHovered = ImGui::IsItemHovered();
            const ImVec2 imgMin = ImGui::GetItemRectMin();
            const ImVec2 imgSize = ImGui::GetItemRectSize();
            const ImVec2 m = ImGui::GetMousePos();
            if (imgSize.x > 0.0f && imgSize.y > 0.0f)
                m_ViewportNDC = Picking::PixelToNDC(glm::vec2(m.x - imgMin.x, m.y - imgMin.y),
                                                    glm::vec2(imgSize.x, imgSize.y));
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
