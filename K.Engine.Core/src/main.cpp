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
#include <Core/SceneComponents.hpp>
#include <Core/QualitySettings.hpp>
#include <Core/Picking.hpp>
#include <Scene/SceneSerializer.hpp>
#include <Script/ScriptBehavior.hpp>
#include <gtc/quaternion.hpp>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace std;

namespace KDot
{
    // -------------------------------------------------------------------------
    // WorldLayer
    //
    //  Procedural LOD terrain + GPU grass demo, now with a small scene editor:
    //  a World Explorer that lists every world item (ECS entity + the
    //  environment singletons) and a Properties panel that edits the selected
    //  item's components. Items can be spawned, duplicated, deleted, picked in
    //  the viewport, and moved with a transform gizmo.
    //
    //  Controls: WASD fly · right-drag look · scroll zoom · left-click =
    //            select (Select tool) or sculpt (Sculpt tool) · R re-drop ball
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
        enum class Tool { Select, Sculpt };
        enum class Sel  { None, Entity, Sun, Fog, Terrain, Grass };
        enum class PlayState { Editing, Playing, Paused };

        Tool        m_Tool = Tool::Select;
        Sel         m_Sel = Sel::None;
        ecs::Entity m_SelEntity = ecs::kNull;
        ecs::Entity m_PendingDelete = ecs::kNull;
        bool        m_LeftPrev = false;
        int         m_NextCube = 1;
        int         m_NextLight = 1;

        PlayState   m_Play = PlayState::Editing;
        std::string m_Snapshot;                 // serialized scene captured on Play
        std::string m_ScenePath = "scene.kscene";

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

            // Point lights are now world items (entities) the explorer can edit.
            SpawnLight(glm::vec3(0.0f, 30.0f, 0.0f),     glm::vec3(1.0f, 0.6f, 0.3f), 2.2f, 160.0f, "Warm Light");
            SpawnLight(glm::vec3(160.0f, 50.0f, -160.0f), glm::vec3(0.3f, 0.7f, 1.0f), 2.0f, 240.0f, "Cool Light");

            SpawnBall();

            // A scripted demo prop: sits still in edit mode, spins on Play.
            {
                ecs::Entity c = m_Registry.Create();
                const glm::vec3 p(40.0f, m_Terrain.HeightAt(40.0f, 40.0f) + 16.0f, 40.0f);
                m_Registry.Emplace<Transform>(c, p);
                m_Registry.Emplace<Prop>(c, Prop{glm::vec3(8.0f), glm::vec4(0.3f, 0.8f, 0.5f, 1.0f)});
                m_Registry.Emplace<KDot::Name>(c, KDot::Name{"Spinner"});
                m_Registry.Emplace<Script>(c).name = "Spin";
            }

            Select(m_Ball);
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
            gp.density   = 0.05f + fid * 0.45f;                         // 0.05 .. 0.5 blades / m^2
            gp.maxBlades = static_cast<int>(8000.0f + fid * 112000.0f); // 8k .. 120k
            gp.minHeight = 1.5f;
            gp.maxHeight = 62.0f;
            gp.slopeThreshold = 0.74f;
            gp.seed = static_cast<std::uint32_t>(m_Seed);

            m_GrassField.Generate(m_Terrain.Field(), gp);
            m_Grass.SetInstances(m_GrassField.Instances());
            m_Grass.maxDistance = 120.0f + fid * 380.0f; // grass view distance scales with fidelity
        }

        // ---- World items ----------------------------------------------------
        void Select(ecs::Entity e)
        {
            m_Sel = Sel::Entity;
            m_SelEntity = e;
        }

        // A point in front of the camera, lifted to sit just above the terrain.
        glm::vec3 SpawnPoint(float dist)
        {
            glm::vec3 p = m_Camera.m_Position + m_Camera.Front() * dist;
            const float h = m_Terrain.HeightAt(p.x, p.z) + 4.0f;
            if (p.y < h)
                p.y = h;
            return p;
        }

        ecs::Entity SpawnCube()
        {
            ecs::Entity e = m_Registry.Create();
            m_Registry.Emplace<Transform>(e, SpawnPoint(55.0f));

            const int id = m_NextCube;
            Prop pr;
            pr.size = glm::vec3(6.0f);
            pr.color = glm::vec4(0.40f + 0.55f * ((id * 37) % 100) / 100.0f,
                                 0.40f + 0.55f * ((id * 71) % 100) / 100.0f,
                                 0.40f + 0.55f * ((id * 53) % 100) / 100.0f, 1.0f);
            m_Registry.Emplace<Prop>(e, pr);
            m_Registry.Emplace<KDot::Name>(e, KDot::Name{"Cube " + std::to_string(m_NextCube++)});
            Select(e);
            return e;
        }

        ecs::Entity SpawnLight(const glm::vec3& pos, const glm::vec3& color, float intensity,
                               float radius, const std::string& name)
        {
            ecs::Entity e = m_Registry.Create();
            m_Registry.Emplace<Transform>(e, pos);
            LightSource ls;
            ls.color = color;
            ls.intensity = intensity;
            ls.radius = radius;
            m_Registry.Emplace<LightSource>(e, ls);
            m_Registry.Emplace<KDot::Name>(e, KDot::Name{name});
            m_NextLight++;
            return e;
        }

        ecs::Entity SpawnLight() // editor "+ Light": in front of the camera
        {
            return SpawnLight(SpawnPoint(45.0f) + glm::vec3(0.0f, 18.0f, 0.0f),
                              glm::vec3(1.0f, 0.85f, 0.6f), 2.0f, 140.0f,
                              "Light " + std::to_string(m_NextLight));
        }

        void DuplicateSelected()
        {
            if (m_Sel != Sel::Entity || !m_Registry.Valid(m_SelEntity))
                return;
            const ecs::Entity s = m_SelEntity;
            ecs::Entity e = m_Registry.Create();

            // Copy each present component through a local first (Emplace may grow
            // the same pool the source lives in, which would dangle a direct ref).
            if (Transform* t = m_Registry.TryGet<Transform>(s))
            {
                Transform nt = *t;
                nt.position += glm::vec3(8.0f, 0.0f, 8.0f);
                m_Registry.Emplace<Transform>(e, nt);
            }
            if (Prop* p = m_Registry.TryGet<Prop>(s))        { Prop c = *p;        m_Registry.Emplace<Prop>(e, c); }
            if (LightSource* l = m_Registry.TryGet<LightSource>(s)) { LightSource c = *l; m_Registry.Emplace<LightSource>(e, c); }
            if (Rigidbody* r = m_Registry.TryGet<Rigidbody>(s)) { Rigidbody c = *r; m_Registry.Emplace<Rigidbody>(e, c); }
            if (Collider* col = m_Registry.TryGet<Collider>(s)) { Collider c = *col; m_Registry.Emplace<Collider>(e, c); }
            if (Script* sc = m_Registry.TryGet<Script>(s))      { m_Registry.Emplace<Script>(e).name = sc->name; } // name only

            std::string base = m_Registry.TryGet<KDot::Name>(s) ? m_Registry.TryGet<KDot::Name>(s)->value : "Entity";
            m_Registry.Emplace<KDot::Name>(e, KDot::Name{base + " copy"});
            Select(e);
        }

        void SpawnBall()
        {
            const bool reselect = (m_Sel == Sel::Entity && m_SelEntity == m_Ball);
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
            m_Registry.Emplace<Prop>(m_Ball, Prop{glm::vec3(6.0f), glm::vec4(0.9f, 0.3f, 0.2f, 1.0f)});
            m_Registry.Emplace<KDot::Name>(m_Ball, KDot::Name{"Physics Ball"});

            if (reselect)
                Select(m_Ball);
        }

        // Rebuild the renderer's point-light list from LightSource entities.
        void SyncLights()
        {
            m_Lights.points.clear();
            m_Registry.View<Transform, LightSource>([&](ecs::Entity, Transform& t, LightSource& ls) {
                m_Lights.points.push_back({t.position, ls.color, ls.intensity, ls.radius});
            });
        }

        // ---- Scene environment <-> serializer -------------------------------
        SceneEnv CaptureEnv() const
        {
            SceneEnv e;
            e.sunDir = m_Lights.sun.direction;
            e.sunColor = m_Lights.sun.color;
            e.sunIntensity = m_Lights.sun.intensity;
            e.ambientColor = m_Lights.ambient.color;
            e.ambientIntensity = m_Lights.ambient.intensity;
            e.fogColor = m_Lights.fogColor;
            e.fogDensity = m_Lights.fogDensity;
            e.terrainSeed = m_Seed;
            e.grassShow = m_ShowGrass;
            e.grassDistance = m_Grass.maxDistance;
            return e;
        }

        void ApplyEnv(const SceneEnv& e)
        {
            m_Lights.sun.direction = glm::normalize(e.sunDir);
            m_Lights.sun.color = e.sunColor;
            m_Lights.sun.intensity = e.sunIntensity;
            m_Lights.ambient.color = e.ambientColor;
            m_Lights.ambient.intensity = e.ambientIntensity;
            m_Lights.fogColor = e.fogColor;
            m_Lights.fogDensity = e.fogDensity;
            m_Seed = e.terrainSeed;
            m_ShowGrass = e.grassShow;
            m_Grass.maxDistance = e.grassDistance;
        }

        // Re-locate the physics ball after the registry was rebuilt (load/stop).
        void RefindBall()
        {
            m_Ball = ecs::kNull;
            m_Registry.View<KDot::Name>([&](ecs::Entity e, KDot::Name& n) {
                if (n.value == "Physics Ball")
                    m_Ball = e;
            });
        }

        void SaveScene() { SceneSerializer::Save(m_ScenePath, m_Registry, CaptureEnv()); }

        void LoadScene()
        {
            SceneEnv env;
            if (!SceneSerializer::Load(m_ScenePath, m_Registry, env))
                return;
            ApplyEnv(env);
            RegenerateTerrain(); // rebuild terrain to the loaded seed
            RefindBall();
            m_Sel = Sel::None;
            m_SelEntity = ecs::kNull;
        }

        // ---- Play mode ------------------------------------------------------
        // Play snapshots the world; Stop restores it. Physics + scripts only run
        // while Playing, so the editor stays a frozen, editable scene.
        void StartPlay()
        {
            m_Snapshot = SceneSerializer::SaveToString(m_Registry, CaptureEnv());
            m_Play = PlayState::Playing;
        }

        void StopPlay()
        {
            SceneEnv env;
            if (SceneSerializer::LoadFromString(m_Snapshot, m_Registry, env))
            {
                ApplyEnv(env);
                RefindBall();
            }
            m_Sel = Sel::None;
            m_SelEntity = ecs::kNull;
            m_Play = PlayState::Editing;
        }

        // Instantiate + tick every Script behaviour for one simulated frame.
        void UpdateScripts(float dt)
        {
            m_Registry.View<Script>([&](ecs::Entity e, Script& s) {
                if (!s.instance && !s.name.empty())
                {
                    s.instance = ScriptRegistry::Get().Create(s.name);
                    if (s.instance)
                        s.instance->Attach(&m_Registry, e);
                }
                if (s.instance && !s.started)
                {
                    s.instance->OnStart();
                    s.started = true;
                }
                if (s.instance)
                    s.instance->OnUpdate(dt);
            });
        }

        bool PickEntity(ecs::Entity& outEntity)
        {
            const Picking::PickRay pr = Picking::ScreenToRay(m_ViewportNDC, m_Camera.GetViewMatrix(), Projection());
            Ray ray{pr.origin, pr.direction};
            RaycastHit hit = m_Physics.Raycast(m_Registry, ray, 6000.0f);
            if (hit.hit && hit.entity != ecs::kNull && m_Registry.Valid(hit.entity))
            {
                outEntity = hit.entity;
                return true;
            }
            return false;
        }

        bool PickTerrain(RaycastHit& outHit)
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

            // --- Left mouse: select (Select tool) or sculpt (Sculpt tool) ---
            const bool leftDown = Input::IsMouseButtonPressed(KDot::Mouse::LeftClick);
            const bool leftClick = leftDown && !m_LeftPrev; // rising edge
            if (m_ViewportHovered)
            {
                if (m_Tool == Tool::Select && leftClick)
                {
                    ecs::Entity picked;
                    if (PickEntity(picked))
                        Select(picked);
                    else
                    {
                        m_Sel = Sel::None;
                        m_SelEntity = ecs::kNull;
                    }
                }
                else if (m_Tool == Tool::Sculpt && leftDown)
                {
                    RaycastHit hit;
                    if (PickTerrain(hit))
                    {
                        const SculptMode mode = Input::IsKeyPressed(KDot::Key::LeftShift) ? SculptMode::Lower : SculptMode::Raise;
                        m_Terrain.Sculpt(glm::vec2(hit.point.x, hit.point.z), m_BrushRadius, m_BrushStrength, mode, dt);
                    }
                }
            }
            m_LeftPrev = leftDown;

            // Physics + scripts only advance while Playing; the editor is frozen.
            const bool simulate = (m_Play == PlayState::Playing);
            m_Terrain.Update(m_Camera.m_Position);
            if (simulate)
            {
                UpdateScripts(dt);
                m_Physics.Step(m_Registry, dt);
            }
            SyncLights();

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

            DrawSceneItems();

            m_Renderer.EndStream();

            // GPU-instanced grass (single draw call), depth-tested against terrain.
            if (m_ShowGrass && m_GrassReady)
                m_Grass.Render(m_Camera.GetViewMatrix(), Projection(), m_Camera.m_Position, m_Time, m_Lights);

            DrawHUD();

            m_Renderer.UnbindFrameBuffer();
        }

        // Draw all renderable world items + a transform gizmo on the selection.
        void DrawSceneItems()
        {
            m_Registry.View<Transform, Prop>([&](ecs::Entity, Transform& t, Prop& p) {
                const float yaw = glm::degrees(glm::eulerAngles(t.rotation).y);
                m_Renderer.DrawCube(t.position, p.size, p.color, yaw);
            });

            // Small emissive markers so lights are visible / locatable in the scene.
            m_Registry.View<Transform, LightSource>([&](ecs::Entity, Transform& t, LightSource& ls) {
                m_Renderer.DrawCube(t.position, glm::vec3(3.0f), glm::vec4(ls.color, 1.0f), 0.0f);
            });

            if (m_Sel == Sel::Entity && m_Registry.Valid(m_SelEntity))
            {
                if (Transform* t = m_Registry.TryGet<Transform>(m_SelEntity))
                {
                    const glm::vec3 o = t->position;
                    const float L = 16.0f, w = 0.8f;
                    m_Renderer.DrawCube(o + glm::vec3(L * 0.5f, 0, 0), glm::vec3(L, w, w), glm::vec4(1.0f, 0.25f, 0.25f, 1.0f), 0.0f);
                    m_Renderer.DrawCube(o + glm::vec3(0, L * 0.5f, 0), glm::vec3(w, L, w), glm::vec4(0.25f, 1.0f, 0.25f, 1.0f), 0.0f);
                    m_Renderer.DrawCube(o + glm::vec3(0, 0, L * 0.5f), glm::vec3(w, w, L), glm::vec4(0.35f, 0.45f, 1.0f, 1.0f), 0.0f);
                }
            }
        }

        void DrawHUD()
        {
            m_Renderer.Begin2D(kFbW, kFbH);

            const glm::vec4 white(1.0f, 1.0f, 1.0f, 0.85f);
            m_Renderer.DrawQuad(glm::vec3(kFbW * 0.5f, kFbH * 0.5f, 0.0f), glm::vec2(44.0f, 4.0f), white);
            m_Renderer.DrawQuad(glm::vec3(kFbW * 0.5f, kFbH * 0.5f, 0.0f), glm::vec2(4.0f, 44.0f), white);

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

        // ====================================================================
        // Editor UI
        // ====================================================================
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

                    ImGuiID center = dockspace_id;
                    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
                    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.32f, nullptr, &center);
                    ImGui::DockBuilderDockWindow("Explorer", left);
                    ImGui::DockBuilderDockWindow("Properties", right);
                    ImGui::DockBuilderDockWindow("Viewport", center);
                    ImGui::DockBuilderFinish(dockspace_id);
                }
            }

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open Scene", "Ctrl+O")) LoadScene();
                    if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveScene();
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Add"))
                {
                    if (ImGui::MenuItem("Cube"))  SpawnCube();
                    if (ImGui::MenuItem("Light")) SpawnLight();
                    ImGui::EndMenu();
                }

                // Transport: Play / Pause / Stop (physics + scripts run only here).
                ImGui::Separator();
                if (m_Play == PlayState::Editing)
                {
                    if (ImGui::Button("> Play"))
                        StartPlay();
                }
                else
                {
                    if (ImGui::Button(m_Play == PlayState::Playing ? "|| Pause" : "> Resume"))
                        m_Play = (m_Play == PlayState::Playing) ? PlayState::Paused : PlayState::Playing;
                    ImGui::SameLine();
                    if (ImGui::Button("[] Stop"))
                        StopPlay();
                }
                ImGui::SameLine();
                ImGui::TextDisabled(m_Play == PlayState::Editing    ? "EDIT"
                                    : m_Play == PlayState::Playing  ? "PLAYING"
                                                                    : "PAUSED");
                ImGui::EndMenuBar();
            }

            DrawExplorer(io);
            DrawProperties();

            // Deferred destroy (so we never free an entity mid-UI).
            if (m_PendingDelete != ecs::kNull)
            {
                if (m_Registry.Valid(m_PendingDelete))
                    m_Registry.Destroy(m_PendingDelete);
                if (m_SelEntity == m_PendingDelete)
                {
                    m_Sel = Sel::None;
                    m_SelEntity = ecs::kNull;
                }
                m_PendingDelete = ecs::kNull;
            }

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

        // ---- World Explorer -------------------------------------------------
        void DrawExplorer(ImGuiIO& io)
        {
            ImGui::Begin("Explorer");

            if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("FPS: %.1f  (%.2f ms)", io.Framerate, io.DeltaTime * 1000.0f);
                ImGui::Text("Draw calls: %d   Tris: %d", m_Renderer.DrawCallCount, m_Renderer.Triangles);

                ImGui::TextUnformatted("Tool:");
                ImGui::SameLine();
                if (ImGui::RadioButton("Select", m_Tool == Tool::Select)) m_Tool = Tool::Select;
                ImGui::SameLine();
                if (ImGui::RadioButton("Sculpt", m_Tool == Tool::Sculpt)) m_Tool = Tool::Sculpt;

                QualitySettings& q = QualitySettings::Get();
                if (ImGui::SliderFloat("Fidelity", &m_Fidelity, 0.0f, 1.0f, "%.2f"))
                    q.SetFidelity(m_Fidelity);
                ImGui::Checkbox("Show grass", &m_ShowGrass);
            }

            ImGui::Separator();
            ImGui::TextDisabled("ENVIRONMENT");
            if (ImGui::Selectable("Sun (Directional)", m_Sel == Sel::Sun))     m_Sel = Sel::Sun;
            if (ImGui::Selectable("Sky & Fog", m_Sel == Sel::Fog))             m_Sel = Sel::Fog;
            if (ImGui::Selectable("Terrain", m_Sel == Sel::Terrain))           m_Sel = Sel::Terrain;
            if (ImGui::Selectable("Grass", m_Sel == Sel::Grass))               m_Sel = Sel::Grass;

            ImGui::Separator();
            ImGui::TextDisabled("ENTITIES");
            if (ImGui::SmallButton("+ Cube"))  SpawnCube();
            ImGui::SameLine();
            if (ImGui::SmallButton("+ Light")) SpawnLight();
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu)", m_Registry.AliveCount());

            // Collect named entities (don't add/destroy while iterating the pool).
            struct Row { ecs::Entity e; std::string label; };
            std::vector<Row> rows;
            m_Registry.View<KDot::Name>([&](ecs::Entity e, KDot::Name& n) {
                const char* type = m_Registry.Has<LightSource>(e) ? "light"
                                 : m_Registry.Has<Rigidbody>(e)   ? "body"
                                 : m_Registry.Has<Prop>(e)        ? "prop"
                                                                  : "node";
                rows.push_back({e, n.value + "  [" + type + "]"});
            });
            std::sort(rows.begin(), rows.end(),
                      [](const Row& a, const Row& b) { return ecs::IndexOf(a.e) < ecs::IndexOf(b.e); });

            ImGui::BeginChild("entities", ImVec2(0, 0), true);
            for (const Row& r : rows)
            {
                const bool sel = (m_Sel == Sel::Entity && m_SelEntity == r.e);
                ImGui::PushID((int)r.e);
                if (ImGui::Selectable(r.label.c_str(), sel))
                    Select(r.e);
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::End();
        }

        // ---- Properties of the selected item --------------------------------
        void DrawProperties()
        {
            ImGui::Begin("Properties");
            switch (m_Sel)
            {
                case Sel::None:    ImGui::TextDisabled("Select an item in the Explorer."); break;
                case Sel::Sun:     DrawSunProps();    break;
                case Sel::Fog:     DrawFogProps();    break;
                case Sel::Terrain: DrawTerrainProps(); break;
                case Sel::Grass:   DrawGrassProps();  break;
                case Sel::Entity:
                    if (m_Registry.Valid(m_SelEntity))
                        DrawEntityProps(m_SelEntity);
                    else
                    {
                        m_Sel = Sel::None;
                        m_SelEntity = ecs::kNull;
                    }
                    break;
            }
            ImGui::End();
        }

        void DrawSunProps()
        {
            ImGui::TextUnformatted("Sun (Directional Light)");
            ImGui::Separator();
            if (ImGui::DragFloat3("Direction", &m_Lights.sun.direction.x, 0.01f, -1.0f, 1.0f))
                m_Lights.sun.direction = glm::normalize(m_Lights.sun.direction);
            ImGui::ColorEdit3("Color", &m_Lights.sun.color.x);
            ImGui::SliderFloat("Intensity", &m_Lights.sun.intensity, 0.0f, 3.0f, "%.2f");
            ImGui::Separator();
            ImGui::TextUnformatted("Ambient");
            ImGui::ColorEdit3("Amb. color", &m_Lights.ambient.color.x);
            ImGui::SliderFloat("Amb. intensity", &m_Lights.ambient.intensity, 0.0f, 1.0f, "%.2f");
        }

        void DrawFogProps()
        {
            ImGui::TextUnformatted("Sky & Fog");
            ImGui::Separator();
            ImGui::ColorEdit3("Fog color", &m_Lights.fogColor.x);
            ImGui::SliderFloat("Fog density", &m_Lights.fogDensity, 0.0f, 0.003f, "%.4f");
        }

        void DrawTerrainProps()
        {
            ImGui::TextUnformatted("Terrain");
            ImGui::Separator();
            ImGui::Text("%zu chunks, %zu tris", m_Terrain.Chunks().size(), m_Terrain.TriangleCount());
            ImGui::InputInt("Seed", &m_Seed);
            if (ImGui::Button("Regenerate"))
                RegenerateTerrain();
            ImGui::Separator();
            ImGui::TextUnformatted("Sculpt brush (use the Sculpt tool)");
            ImGui::SliderFloat("Radius", &m_BrushRadius, 4.0f, 100.0f, "%.0f");
            ImGui::SliderFloat("Strength", &m_BrushStrength, 1.0f, 60.0f, "%.0f");
            ImGui::TextDisabled("left-click sculpt (hold Shift = lower)");
        }

        void DrawGrassProps()
        {
            ImGui::TextUnformatted("Grass");
            ImGui::Separator();
            ImGui::Checkbox("Visible", &m_ShowGrass);
            ImGui::Text("%d blades (1 draw call)", m_Grass.InstanceCount());
            ImGui::DragFloat("View distance", &m_Grass.maxDistance, 1.0f, 20.0f, 800.0f, "%.0f");
            ImGui::DragFloat("Blade height", &m_Grass.bladeHeight, 0.01f, 0.1f, 6.0f, "%.2f");
            ImGui::DragFloat("Blade width", &m_Grass.bladeWidth, 0.005f, 0.02f, 1.0f, "%.3f");
        }

        // Component-by-component editor for a single entity.
        enum class Rem { None, Prop, Light, Rb, Col, Scr };

        void DrawEntityProps(ecs::Entity e)
        {
            ImGui::PushID((int)e);

            // Name
            if (KDot::Name* n = m_Registry.TryGet<KDot::Name>(e))
            {
                char buf[128];
                std::strncpy(buf, n->value.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("Name", buf, sizeof(buf)))
                    n->value = buf;
            }
            ImGui::TextDisabled("entity #%u", ecs::IndexOf(e));
            ImGui::Separator();

            Rem toRemove = Rem::None;

            if (Transform* t = m_Registry.TryGet<Transform>(e))
            {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat3("Position", &t->position.x, 0.25f);
                    glm::vec3 euler = glm::degrees(glm::eulerAngles(t->rotation));
                    if (ImGui::DragFloat3("Rotation", &euler.x, 1.0f))
                        t->rotation = glm::quat(glm::radians(euler));
                    ImGui::DragFloat3("Scale", &t->scale.x, 0.05f, 0.01f, 1000.0f);
                }
            }

            if (Prop* p = m_Registry.TryGet<Prop>(e))
            {
                if (ImGui::CollapsingHeader("Prop (box)", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::ColorEdit4("Color", &p->color.x);
                    ImGui::DragFloat3("Size", &p->size.x, 0.1f, 0.01f, 1000.0f);
                    if (ImGui::SmallButton("Remove Prop")) toRemove = Rem::Prop;
                }
            }

            if (LightSource* ls = m_Registry.TryGet<LightSource>(e))
            {
                if (ImGui::CollapsingHeader("Light Source", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::ColorEdit3("Color", &ls->color.x);
                    ImGui::DragFloat("Intensity", &ls->intensity, 0.05f, 0.0f, 20.0f);
                    ImGui::DragFloat("Radius", &ls->radius, 1.0f, 1.0f, 2000.0f);
                    if (ImGui::SmallButton("Remove Light")) toRemove = Rem::Light;
                }
            }

            if (Rigidbody* rb = m_Registry.TryGet<Rigidbody>(e))
            {
                if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    float mass = rb->mass;
                    if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.0f, 1000.0f))
                        rb->SetMass(mass);
                    ImGui::SliderFloat("Restitution", &rb->restitution, 0.0f, 1.0f);
                    ImGui::SliderFloat("Friction", &rb->friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Lin. damping", &rb->linearDamping, 0.0f, 1.0f);
                    ImGui::Checkbox("Use gravity", &rb->useGravity);
                    bool isStatic = rb->isStatic;
                    if (ImGui::Checkbox("Static", &isStatic))
                        rb->SetStatic(isStatic);
                    ImGui::Text("Velocity: %.1f, %.1f, %.1f", rb->velocity.x, rb->velocity.y, rb->velocity.z);
                    ImGui::Text("On ground: %s", rb->onGround ? "yes" : "no");
                    if (ImGui::SmallButton("Remove Rigidbody")) toRemove = Rem::Rb;
                }
            }

            if (Collider* col = m_Registry.TryGet<Collider>(e))
            {
                if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const char* types[] = {"Box", "Sphere"};
                    int ti = (int)col->type;
                    if (ImGui::Combo("Shape", &ti, types, 2))
                        col->type = (ColliderType)ti;
                    if (col->type == ColliderType::Box)
                        ImGui::DragFloat3("Half extents", &col->halfExtents.x, 0.1f, 0.01f, 1000.0f);
                    else
                        ImGui::DragFloat("Radius", &col->radius, 0.1f, 0.01f, 1000.0f);
                    ImGui::DragFloat3("Offset", &col->localOffset.x, 0.1f);
                    ImGui::Checkbox("Trigger", &col->isTrigger);
                    if (ImGui::SmallButton("Remove Collider")) toRemove = Rem::Col;
                }
            }

            if (Script* sc = m_Registry.TryGet<Script>(e))
            {
                if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const std::string current = sc->name.empty() ? "<none>" : sc->name;
                    if (ImGui::BeginCombo("Behaviour", current.c_str()))
                    {
                        for (const std::string& nm : ScriptRegistry::Get().Names())
                        {
                            if (ImGui::Selectable(nm.c_str(), nm == sc->name))
                            {
                                sc->name = nm;
                                sc->instance.reset(); // rebind on next play
                                sc->started = false;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::TextDisabled("%s", sc->instance ? "running"
                                              : (m_Play == PlayState::Editing ? "idle (press Play)" : "not started"));
                    if (ImGui::SmallButton("Remove Script")) toRemove = Rem::Scr;
                }
            }

            switch (toRemove)
            {
                case Rem::Prop:  m_Registry.Remove<Prop>(e); break;
                case Rem::Light: m_Registry.Remove<LightSource>(e); break;
                case Rem::Rb:    m_Registry.Remove<Rigidbody>(e); break;
                case Rem::Col:   m_Registry.Remove<Collider>(e); break;
                case Rem::Scr:   m_Registry.Remove<Script>(e); break;
                case Rem::None:  break;
            }

            ImGui::Separator();
            ImGui::TextDisabled("Add component:");
            if (!m_Registry.Has<Prop>(e))        { if (ImGui::Button("Prop"))      m_Registry.Emplace<Prop>(e); ImGui::SameLine(); }
            if (!m_Registry.Has<LightSource>(e)) { if (ImGui::Button("Light"))     m_Registry.Emplace<LightSource>(e); ImGui::SameLine(); }
            if (!m_Registry.Has<Rigidbody>(e))   { if (ImGui::Button("Rigidbody")) m_Registry.Emplace<Rigidbody>(e); ImGui::SameLine(); }
            if (!m_Registry.Has<Script>(e))      { if (ImGui::Button("Script"))    m_Registry.Emplace<Script>(e); ImGui::SameLine(); }
            if (!m_Registry.Has<Collider>(e))    { if (ImGui::Button("Collider"))  m_Registry.Emplace<Collider>(e, Collider::MakeBox(glm::vec3(3.0f))); }
            ImGui::NewLine();

            ImGui::Separator();
            if (ImGui::Button("Duplicate"))
                DuplicateSelected();
            ImGui::SameLine();
            if (ImGui::Button("Delete"))
                m_PendingDelete = e;

            ImGui::PopID();
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
