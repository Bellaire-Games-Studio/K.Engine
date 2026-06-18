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
#include <Core/Hierarchy.hpp>
#include <Core/QualitySettings.hpp>
#include <Core/Picking.hpp>
#include <Scene/SceneSerializer.hpp>
#include <Script/ScriptBehavior.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#if defined(__has_include)
#  if __has_include(<filesystem>)
#    include <filesystem>
#    define KE_HAS_FILESYSTEM 1
#  endif
#endif

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
    //  Controls: WASD fly · right-drag look · scroll zoom · left-click drives the
    //            active toolbar tool (select / move-rotate-scale gizmo / sculpt
    //            brush) · R re-drop ball
    // -------------------------------------------------------------------------
    namespace
    {
        // Draws a behaviour's declared parameters as editable widgets.
        struct ImGuiParams : ScriptParams
        {
            void Float(const char* n, float& v, float mn, float mx) override
            {
                if (mx > mn) ImGui::SliderFloat(n, &v, mn, mx);
                else         ImGui::DragFloat(n, &v, 0.1f);
            }
            void Int(const char* n, int& v) override { ImGui::DragInt(n, &v); }
            void Bool(const char* n, bool& v) override { ImGui::Checkbox(n, &v); }
            void Vec3(const char* n, glm::vec3& v) override { ImGui::DragFloat3(n, &v.x, 0.05f); }
        };

        // World-space axes the transform gizmo manipulates along.
        const glm::vec3 kGizmoAxis[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

        // Closest parameter 'tAxis' along the infinite axis line (P0 + axis*t) to
        // the cursor ray (O + dir*s), plus the distance between the two lines.
        // Used both to grab a gizmo handle and to drag along it. Returns false if
        // the ray is parallel to the axis.
        bool ClosestAxisParam(const glm::vec3& O, const glm::vec3& dir,
                              const glm::vec3& P0, const glm::vec3& axis,
                              float& tAxis, float& lineDist)
        {
            const glm::vec3 w0 = O - P0;
            const float a = glm::dot(dir, dir);
            const float b = glm::dot(dir, axis);
            const float c = glm::dot(axis, axis);
            const float d = glm::dot(dir, w0);
            const float e = glm::dot(axis, w0);
            const float denom = a * c - b * b;
            if (std::abs(denom) < 1e-6f)
                return false;
            const float s = (b * e - c * d) / denom; // param along the ray
            const float t = (a * e - b * d) / denom; // param along the axis
            const glm::vec3 pRay = O + dir * s;
            const glm::vec3 pAxis = P0 + axis * t;
            tAxis = t;
            lineDist = glm::length(pRay - pAxis);
            return true;
        }

        // A templated C++ behaviour the in-editor Script editor writes to disk.
        std::string ScriptTemplate(const std::string& cls)
        {
            return
"#include <Script/ScriptBehavior.hpp>\n"
"#include <gtc/quaternion.hpp>\n"
"#include <cmath>\n"
"\n"
"// A custom C++ behaviour. OnUpdate runs every simulated frame while playing;\n"
"// declare tweakable fields in OnInspect so they appear in the inspector and\n"
"// save with the scene. Drop this file in Scripts/ and rebuild to compile it in\n"
"// (the same code then runs on native and web).\n"
"namespace KDot\n"
"{\n"
"    class " + cls + " : public ScriptBehavior\n"
"    {\n"
"    public:\n"
"        void OnInspect(ScriptParams& p) override\n"
"        {\n"
"            p.Float(\"speed\", m_Speed, 0.0f, 360.0f);\n"
"        }\n"
"\n"
"        void OnStart() override {}\n"
"\n"
"        void OnUpdate(float dt) override\n"
"        {\n"
"            if (Transform* t = GetTransform())\n"
"                t->rotation = glm::angleAxis(glm::radians(m_Speed * dt),\n"
"                                             glm::vec3(0, 1, 0)) * t->rotation;\n"
"        }\n"
"\n"
"    private:\n"
"        float m_Speed = 45.0f;\n"
"    };\n"
"\n"
"    KE_REGISTER_SCRIPT(" + cls + ", \"" + cls + "\")\n"
"}\n";
        }
    }

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
        //  The toolbar exposes the transform gizmo tools (Select/Move/Rotate/
        //  Scale) and the terrain sculpt brushes (Raise/Lower/Flatten/Smooth)
        //  side by side, so "sculpting with different tools" lives next to object
        //  manipulation again.
        enum class Tool { Select, Move, Rotate, Scale,
                          SculptRaise, SculptLower, SculptFlatten, SculptSmooth };
        enum class Sel  { None, Entity, Sun, Fog, Terrain, Grass, Post };
        enum class PlayState { Editing, Playing, Paused };

        static bool IsTransformTool(Tool t) { return t == Tool::Move || t == Tool::Rotate || t == Tool::Scale; }
        static bool IsSculptTool(Tool t)    { return t >= Tool::SculptRaise; }
        static SculptMode SculptModeOf(Tool t)
        {
            switch (t)
            {
                case Tool::SculptLower:   return SculptMode::Lower;
                case Tool::SculptFlatten: return SculptMode::Flatten;
                case Tool::SculptSmooth:  return SculptMode::Smooth;
                default:                  return SculptMode::Raise;
            }
        }

        Tool        m_Tool = Tool::Select;
        Sel         m_Sel = Sel::None;
        ecs::Entity m_SelEntity = ecs::kNull;
        ecs::Entity m_PendingDelete = ecs::kNull;
        bool        m_LeftPrev = false;
        int         m_NextCube = 1;
        int         m_NextLight = 1;

        // Transform-gizmo drag state.
        int       m_HoverAxis = -1;   // axis under the cursor (highlight)
        int       m_GrabAxis  = -1;   // axis currently being dragged (-1 = none)
        bool      m_Dragging  = false;
        float     m_GizmoScale = 16.0f;
        glm::vec3 m_DragOrigin{0.0f};      // fixed gizmo origin captured at grab
        float     m_DragStartParam = 0.0f; // axis parameter where the grab happened
        glm::vec3 m_DragStartPos{0.0f};    // entity local position at grab
        glm::quat m_DragStartRot{1, 0, 0, 0};
        glm::vec3 m_DragStartScale{1.0f};
        glm::vec2 m_DragStartMouse{0.0f};

        // Grass view-distance culling (only blades near the camera are uploaded).
        std::vector<GrassInstance> m_GrassVisible;
        glm::vec3                  m_GrassCullPos{1e9f};

        // In-editor C++ script authoring.
        bool                     m_ShowScriptEditor = false;
        std::vector<char>        m_ScriptBuf;
        char                     m_ScriptName[64] = "MyBehaviour";
        std::string              m_ScriptStatus;
        std::vector<std::string> m_ScriptSaved; // files written this session

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

            // Parent/child demo: a barrel parented to a spinning turret base. On
            // Play the base spins (Spin script) and the barrel orbits with it,
            // because its Transform is relative to the parent.
            {
                ecs::Entity base = m_Registry.Create();
                const glm::vec3 bp(-60.0f, m_Terrain.HeightAt(-60.0f, -60.0f) + 8.0f, -60.0f);
                m_Registry.Emplace<Transform>(base, bp);
                m_Registry.Emplace<Prop>(base, Prop{glm::vec3(10.0f, 4.0f, 10.0f), glm::vec4(0.5f, 0.5f, 0.6f, 1.0f)});
                m_Registry.Emplace<KDot::Name>(base, KDot::Name{"Turret"});
                m_Registry.Emplace<Script>(base).name = "Spin";

                ecs::Entity barrel = m_Registry.Create();
                m_Registry.Emplace<Transform>(barrel, glm::vec3(0.0f, 3.0f, 9.0f)); // local to base
                m_Registry.Emplace<Prop>(barrel, Prop{glm::vec3(2.0f, 2.0f, 12.0f), glm::vec4(0.85f, 0.3f, 0.2f, 1.0f)});
                m_Registry.Emplace<KDot::Name>(barrel, KDot::Name{"Barrel"});
                m_Registry.Emplace<Parent>(barrel).value = base;
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
            m_Grass.maxDistance = 120.0f + fid * 380.0f; // grass view distance scales with fidelity
            CullGrass(true);
        }

        // Upload only the blades within the grass view distance of the camera, so
        // the GPU never processes the whole field every frame. Recomputed only
        // when the camera has moved enough (it's stable while standing still),
        // which keeps the per-frame cost near zero.
        void CullGrass(bool force)
        {
            const glm::vec3 cam = m_Camera.m_Position;
            if (!force && glm::length(cam - m_GrassCullPos) < 10.0f)
                return;
            m_GrassCullPos = cam;

            const float maxD = m_Grass.maxDistance + 16.0f;
            const float maxD2 = maxD * maxD;
            const std::vector<GrassInstance>& all = m_GrassField.Instances();
            m_GrassVisible.clear();
            m_GrassVisible.reserve(all.size());
            for (const GrassInstance& g : all)
            {
                const float dx = g.position.x - cam.x;
                const float dz = g.position.z - cam.z;
                if (dx * dx + dz * dz <= maxD2)
                    m_GrassVisible.push_back(g);
            }
            m_Grass.SetInstances(m_GrassVisible);
        }

        // Gizmo size: scales with the selected object (per the request) but with a
        // distance-based floor so a tiny or far object's handles stay grabbable.
        float GizmoScaleFor(ecs::Entity e)
        {
            const glm::vec3 o = WorldPosition(m_Registry, e);
            glm::vec3 ext(1.0f);
            if (Transform* t = m_Registry.TryGet<Transform>(e))
                ext = glm::abs(t->scale);
            if (Prop* p = m_Registry.TryGet<Prop>(e))
                ext *= glm::abs(p->size);
            const float objSize = glm::max(glm::max(ext.x, ext.y), ext.z);
            const float distFloor = glm::length(m_Camera.m_Position - o) * 0.05f;
            return glm::clamp(objSize * 1.2f + distFloor * 0.5f, distFloor, 4000.0f);
        }

        // Parent's world matrix (identity if unparented) - used to convert gizmo
        // edits in world space back into the entity's local Transform.
        glm::mat4 ParentWorld(ecs::Entity e)
        {
            if (Parent* p = m_Registry.TryGet<Parent>(e))
                if (p->value != ecs::kNull && m_Registry.Valid(p->value))
                    return WorldMatrix(m_Registry, p->value);
            return glm::mat4(1.0f);
        }

        // Pick the gizmo axis (0/1/2) closest to the cursor ray, or -1 if none is
        // within grabbing distance. Outputs the axis parameter at the grab point.
        int PickGizmoAxis(const glm::vec3& o, float g, const Picking::PickRay& ray, float& outParam)
        {
            int best = -1;
            float bestDist = 1e9f;
            outParam = 0.0f;
            const float grabR = g * 0.18f;
            for (int i = 0; i < 3; ++i)
            {
                float t = 0.0f, dist = 0.0f;
                if (!ClosestAxisParam(ray.origin, ray.direction, o, kGizmoAxis[i], t, dist))
                    continue;
                if (t < 0.0f || t > g)
                    continue; // only along the drawn handle
                if (dist < grabR && dist < bestDist)
                {
                    bestDist = dist;
                    best = i;
                    outParam = t;
                }
            }
            return best;
        }

        // Apply the active transform tool while a gizmo handle is held.
        void ApplyGizmoDrag(Transform* t, const Picking::PickRay& ray, const glm::vec2& mouse)
        {
            const int axis = m_GrabAxis;
            if (axis < 0 || !t)
                return;
            const glm::vec3 A = kGizmoAxis[axis];

            if (m_Tool == Tool::Move)
            {
                float tNow = 0.0f, dd = 0.0f;
                if (ClosestAxisParam(ray.origin, ray.direction, m_DragOrigin, A, tNow, dd))
                {
                    const glm::vec3 newWorld = m_DragOrigin + A * (tNow - m_DragStartParam);
                    t->position = glm::vec3(glm::affineInverse(ParentWorld(m_SelEntity)) * glm::vec4(newWorld, 1.0f));
                }
            }
            else if (m_Tool == Tool::Scale)
            {
                float tNow = 0.0f, dd = 0.0f;
                if (ClosestAxisParam(ray.origin, ray.direction, m_DragOrigin, A, tNow, dd))
                {
                    const float ref = (std::abs(m_DragStartParam) > 1e-2f) ? m_DragStartParam : (m_GizmoScale * 0.5f);
                    const float ratio = glm::clamp(tNow / ref, 0.05f, 50.0f);
                    glm::vec3 s = m_DragStartScale;
                    s[axis] = glm::max(m_DragStartScale[axis] * ratio, 0.001f);
                    t->scale = s;
                }
            }
            else if (m_Tool == Tool::Rotate)
            {
                const float dxPix = mouse.x - m_DragStartMouse.x;
                t->rotation = glm::angleAxis(glm::radians(dxPix * 0.4f), A) * m_DragStartRot;
            }
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
            if (Parent* pr = m_Registry.TryGet<Parent>(s)) { ecs::Entity pv = pr->value; m_Registry.Emplace<Parent>(e).value = pv; }
            if (Prop* p = m_Registry.TryGet<Prop>(s))        { Prop c = *p;        m_Registry.Emplace<Prop>(e, c); }
            if (LightSource* l = m_Registry.TryGet<LightSource>(s)) { LightSource c = *l; m_Registry.Emplace<LightSource>(e, c); }
            if (Rigidbody* r = m_Registry.TryGet<Rigidbody>(s)) { Rigidbody c = *r; m_Registry.Emplace<Rigidbody>(e, c); }
            if (Collider* col = m_Registry.TryGet<Collider>(s)) { Collider c = *col; m_Registry.Emplace<Collider>(e, c); }
            if (Script* sc = m_Registry.TryGet<Script>(s))
            {
                // Capture before Emplace (which may grow the Script pool); the
                // behaviour instance is heap-owned, so this raw pointer survives.
                const std::string  sname = sc->name;
                ScriptBehavior*    src   = sc->instance.get();
                Script& ns = m_Registry.Emplace<Script>(e);
                ns.name = sname;
                ns.instance = ScriptRegistry::Get().Create(sname);
                if (ns.instance)
                {
                    ns.instance->Attach(&m_Registry, e);
                    if (src)
                        SceneSerializer::CopyScriptParams(*src, *ns.instance);
                }
            }

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
            m_Registry.View<Transform, LightSource>([&](ecs::Entity e, Transform&, LightSource& ls) {
                m_Lights.points.push_back({WorldPosition(m_Registry, e), ls.color, ls.intensity, ls.radius});
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

        // (Re)create a behaviour instance with default params for an entity.
        void AttachScript(ecs::Entity e, const std::string& name)
        {
            Script& s = m_Registry.Emplace<Script>(e);
            s.name = name;
            s.started = false;
            s.instance = ScriptRegistry::Get().Create(name);
            if (s.instance)
                s.instance->Attach(&m_Registry, e);
        }

        // Make sure every named script has a live instance, so its parameters can
        // be edited and serialized even while the scene is only being edited.
        void EnsureScriptInstances()
        {
            m_Registry.View<Script>([&](ecs::Entity e, Script& s) {
                if (!s.instance && !s.name.empty())
                {
                    s.instance = ScriptRegistry::Get().Create(s.name);
                    if (s.instance)
                        s.instance->Attach(&m_Registry, e);
                }
            });
        }

        // Tick every Script behaviour for one simulated frame.
        void UpdateScripts(float dt)
        {
            m_Registry.View<Script>([&](ecs::Entity e, Script& s) {
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

            // --- Left mouse: transform gizmo / select / sculpt (per active tool) ---
            const bool leftDown = Input::IsMouseButtonPressed(KDot::Mouse::LeftClick);
            const bool leftClick = leftDown && !m_LeftPrev; // rising edge
            m_HoverAxis = -1;

            if (m_ViewportHovered || m_Dragging)
            {
                const Picking::PickRay ray = Picking::ScreenToRay(m_ViewportNDC, m_Camera.GetViewMatrix(), Projection());
                const bool haveXform = (m_Sel == Sel::Entity && m_Registry.Valid(m_SelEntity)
                                        && m_Registry.TryGet<Transform>(m_SelEntity));

                if (IsTransformTool(m_Tool) && haveXform)
                {
                    Transform* t = m_Registry.TryGet<Transform>(m_SelEntity);
                    const glm::vec3 o = WorldPosition(m_Registry, m_SelEntity);
                    m_GizmoScale = GizmoScaleFor(m_SelEntity);

                    if (m_Dragging && leftDown)
                    {
                        ApplyGizmoDrag(t, ray, mouse);
                    }
                    else if (leftClick)
                    {
                        float grabParam = 0.0f;
                        const int axis = PickGizmoAxis(o, m_GizmoScale, ray, grabParam);
                        if (axis >= 0)
                        {
                            m_Dragging = true;
                            m_GrabAxis = axis;
                            m_DragOrigin = o;
                            m_DragStartParam = grabParam;
                            m_DragStartPos = t->position;
                            m_DragStartRot = t->rotation;
                            m_DragStartScale = t->scale;
                            m_DragStartMouse = mouse;
                        }
                        else // missed the handles: re-pick (or clear) like Select
                        {
                            ecs::Entity picked;
                            if (PickEntity(picked)) Select(picked);
                            else { m_Sel = Sel::None; m_SelEntity = ecs::kNull; }
                        }
                    }
                    else
                    {
                        float dummy = 0.0f;
                        m_HoverAxis = PickGizmoAxis(o, m_GizmoScale, ray, dummy);
                    }
                }
                else if (m_Tool == Tool::Select && leftClick)
                {
                    ecs::Entity picked;
                    if (PickEntity(picked)) Select(picked);
                    else { m_Sel = Sel::None; m_SelEntity = ecs::kNull; }
                }
                else if (IsSculptTool(m_Tool) && leftDown)
                {
                    RaycastHit hit;
                    if (PickTerrain(hit))
                        m_Terrain.Sculpt(glm::vec2(hit.point.x, hit.point.z),
                                         m_BrushRadius, m_BrushStrength, SculptModeOf(m_Tool), dt);
                }
            }

            if (!leftDown)
            {
                m_Dragging = false;
                m_GrabAxis = -1;
            }
            m_LeftPrev = leftDown;

            // Physics + scripts only advance while Playing; the editor is frozen.
            const bool simulate = (m_Play == PlayState::Playing);
            EnsureScriptInstances(); // so params are live for the inspector/save
            m_Terrain.Update(m_Camera.m_Position);
            CullGrass(false);        // refresh the near-camera grass set if we moved
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

            // Map the HDR scene down to the 8-bit texture the viewport shows
            // (exposure + tonemap operator + sRGB). The HUD is included so the
            // crosshair tonemaps with the scene; "None" reproduces the old look.
            m_Renderer.ResolveToneMap();
        }

        // Draw all renderable world items + a transform gizmo on the selection.
        void DrawSceneItems()
        {
            // Props render at their *world* transform (Transform composed up the
            // parent chain), so parented parts move with their parent.
            m_Registry.View<Transform, Prop>([&](ecs::Entity e, Transform&, Prop& p) {
                const glm::mat4 world = WorldMatrix(m_Registry, e) * glm::scale(glm::mat4(1.0f), p.size);
                m_Renderer.DrawCube(world, p.color);
            });

            // Small emissive markers so lights are visible / locatable in the scene.
            m_Registry.View<Transform, LightSource>([&](ecs::Entity e, Transform&, LightSource& ls) {
                m_Renderer.DrawCube(WorldPosition(m_Registry, e), glm::vec3(3.0f), glm::vec4(ls.color, 1.0f), 0.0f);
            });

            if (m_Sel == Sel::Entity && m_Registry.Valid(m_SelEntity) && m_Registry.TryGet<Transform>(m_SelEntity))
                DrawGizmo(m_SelEntity);
        }

        // Transform gizmo: three axis handles scaled to the selected object, the
        // active/hovered axis highlighted. Move/Scale tools drag along a handle;
        // Rotate spins about it. A passive locator is drawn for non-transform
        // tools so the selection is always findable.
        void DrawGizmo(ecs::Entity e)
        {
            const glm::vec3 o = WorldPosition(m_Registry, e);
            const float g = GizmoScaleFor(e);
            m_GizmoScale = g;
            const float bar = g * 0.045f; // handle thickness
            const float tip = g * 0.13f;  // end-cap size
            const bool  tf  = IsTransformTool(m_Tool);

            const glm::vec4 axisCol[3] = {
                {0.95f, 0.26f, 0.26f, 1.0f}, {0.30f, 0.95f, 0.32f, 1.0f}, {0.34f, 0.48f, 1.0f, 1.0f}};
            const glm::vec4 hot(1.0f, 0.85f, 0.15f, 1.0f);

            for (int i = 0; i < 3; ++i)
            {
                glm::vec4 col = axisCol[i];
                if (tf && (m_GrabAxis == i || (m_GrabAxis < 0 && m_HoverAxis == i)))
                    col = hot;

                const glm::vec3 a = kGizmoAxis[i];
                glm::vec3 size(bar);
                size[i] = g;                          // long along its axis
                m_Renderer.DrawCube(o + a * (g * 0.5f), size, col, 0.0f); // shaft
                m_Renderer.DrawCube(o + a * g, glm::vec3(tip), col, 0.0f); // cap
            }
            m_Renderer.DrawCube(o, glm::vec3(g * 0.08f), glm::vec4(0.95f, 0.95f, 0.95f, 1.0f), 0.0f);
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
                    ImGui::DockBuilderDockWindow("Script Editor", center); // tab over the viewport
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
                if (ImGui::BeginMenu("Scripts"))
                {
                    ImGui::MenuItem("C++ Script Editor", nullptr, &m_ShowScriptEditor);
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
            if (m_ShowScriptEditor)
                DrawScriptEditor();

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
            DrawToolbar(); // scene-interaction tools across the top of the view
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
                ImGui::Text("Grass: %d blades drawn", m_Grass.InstanceCount());
                ImGui::TextDisabled("Scene tools are on the viewport toolbar.");

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
            if (ImGui::Selectable("Rendering (HDR / Tonemap)", m_Sel == Sel::Post)) m_Sel = Sel::Post;

            ImGui::Separator();
            ImGui::TextDisabled("ENTITIES");
            if (ImGui::SmallButton("+ Cube"))  SpawnCube();
            ImGui::SameLine();
            if (ImGui::SmallButton("+ Light")) SpawnLight();
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu)", m_Registry.AliveCount());

            // Build the parent -> children adjacency (roots = no valid parent),
            // then draw the hierarchy as a tree.
            std::vector<ecs::Entity> roots;
            std::unordered_map<ecs::Entity, std::vector<ecs::Entity>> children;
            m_Registry.View<KDot::Name>([&](ecs::Entity e, KDot::Name&) {
                Parent* p = m_Registry.TryGet<Parent>(e);
                if (p && p->value != ecs::kNull && m_Registry.Valid(p->value))
                    children[p->value].push_back(e);
                else
                    roots.push_back(e);
            });
            auto byIndex = [](ecs::Entity a, ecs::Entity b) { return ecs::IndexOf(a) < ecs::IndexOf(b); };
            std::sort(roots.begin(), roots.end(), byIndex);
            for (auto& kv : children) std::sort(kv.second.begin(), kv.second.end(), byIndex);

            ImGui::BeginChild("entities", ImVec2(0, 0), true);
            std::function<void(ecs::Entity)> drawNode = [&](ecs::Entity e) {
                KDot::Name* n = m_Registry.TryGet<KDot::Name>(e);
                const char* type = m_Registry.Has<LightSource>(e) ? "light"
                                 : m_Registry.Has<Rigidbody>(e)   ? "body"
                                 : m_Registry.Has<Prop>(e)        ? "prop"
                                                                  : "node";
                const bool hasKids = children.count(e) && !children[e].empty();
                const std::string label = (n ? n->value : std::string("entity")) + "  [" + type + "]";

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                if (m_Sel == Sel::Entity && m_SelEntity == e) flags |= ImGuiTreeNodeFlags_Selected;
                if (!hasKids) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                ImGui::PushID((int)e);
                const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    Select(e);
                if (open && hasKids)
                {
                    for (ecs::Entity c : children[e]) drawNode(c);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            };
            for (ecs::Entity r : roots) drawNode(r);
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
                case Sel::Post:    DrawPostProps();   break;
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
            ImGui::TextUnformatted("Sculpt brush");
            ImGui::SliderFloat("Radius", &m_BrushRadius, 4.0f, 100.0f, "%.0f");
            ImGui::SliderFloat("Strength", &m_BrushStrength, 1.0f, 60.0f, "%.0f");
            ImGui::TextDisabled("Pick Raise / Lower / Flatten / Smooth on the toolbar, then left-click-drag.");
        }

        void DrawPostProps()
        {
            ImGui::TextUnformatted("HDR / Tonemapping");
            ImGui::Separator();
            const char* modes[] = {"None (linear)", "ACES (filmic)", "Reinhard", "Filmic (Hejl)"};
            ImGui::Combo("Operator", &m_Renderer.tonemapMode, modes, IM_ARRAYSIZE(modes));
            ImGui::SliderFloat("Exposure", &m_Renderer.tonemapExposure, 0.1f, 4.0f, "%.2f");
            ImGui::Separator();
            ImGui::TextDisabled("Scene buffer: %s", m_Renderer.HdrEnabled() ? "RGBA16F (HDR)"
                                                                            : "RGBA8 (float unsupported)");
            ImGui::TextWrapped("The scene renders to a float buffer; this pass applies exposure, the "
                               "chosen curve, and sRGB. Pick \"None\" to compare against the raw look.");
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

            // Parent selector (re-parenting keeps the entity's world transform).
            {
                Parent* pp = m_Registry.TryGet<Parent>(e);
                const ecs::Entity curParent = pp ? pp->value : ecs::kNull;
                std::string label = "<none>";
                if (curParent != ecs::kNull && m_Registry.Valid(curParent))
                    if (KDot::Name* pn = m_Registry.TryGet<KDot::Name>(curParent))
                        label = pn->value;
                if (ImGui::BeginCombo("Parent", label.c_str()))
                {
                    if (ImGui::Selectable("<none>", curParent == ecs::kNull))
                        SetParentKeepWorld(m_Registry, e, ecs::kNull);
                    m_Registry.View<KDot::Name>([&](ecs::Entity cand, KDot::Name& cn) {
                        if (cand == e || IsAncestor(m_Registry, e, cand))
                            return; // skip self / would-be cycle
                        if (ImGui::Selectable((cn.value + "##" + std::to_string(cand)).c_str(), cand == curParent))
                            SetParentKeepWorld(m_Registry, e, cand);
                    });
                    ImGui::EndCombo();
                }
            }
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
                            if (ImGui::Selectable(nm.c_str(), nm == sc->name))
                                AttachScript(e, nm); // fresh instance w/ default params
                        ImGui::EndCombo();
                    }
                    // Script-authored parameters (declared via OnInspect).
                    if (sc->instance)
                    {
                        ImGui::Separator();
                        ImGui::TextDisabled("Parameters");
                        ImGuiParams vis;
                        sc->instance->OnInspect(vis);
                    }
                    ImGui::TextDisabled("%s", sc->started ? "running"
                                              : (m_Play == PlayState::Editing ? "idle (press Play)" : "ready"));
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

        // ---- Viewport toolbar ----------------------------------------------
        void DrawToolbar()
        {
            auto btn = [&](const char* label, Tool t, const char* tip) {
                const bool active = (m_Tool == t);
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.52f, 0.92f, 1.0f));
                }
                if (ImGui::Button(label))
                    m_Tool = t;
                if (active)
                    ImGui::PopStyleColor(2);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", tip);
                ImGui::SameLine();
            };

            ImGui::TextUnformatted("Tools:");
            ImGui::SameLine();
            btn("Select", Tool::Select, "Click to pick objects in the viewport");
            btn("Move",   Tool::Move,   "Drag an axis handle to move the selection");
            btn("Rotate", Tool::Rotate, "Drag left/right to rotate about the picked axis");
            btn("Scale",  Tool::Scale,  "Drag an axis handle to scale the selection");

            ImGui::TextUnformatted("| Sculpt:");
            ImGui::SameLine();
            btn("Raise",   Tool::SculptRaise,   "Raise terrain under the brush");
            btn("Lower",   Tool::SculptLower,   "Lower terrain under the brush");
            btn("Flatten", Tool::SculptFlatten, "Pull terrain toward the brush's average height");
            btn("Smooth",  Tool::SculptSmooth,  "Smooth / blur terrain under the brush");

            if (IsSculptTool(m_Tool))
            {
                ImGui::SetNextItemWidth(120.0f);
                ImGui::SliderFloat("##brushR", &m_BrushRadius, 4.0f, 100.0f, "radius %.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                ImGui::SliderFloat("##brushS", &m_BrushStrength, 1.0f, 60.0f, "strength %.0f");
            }
            else
            {
                ImGui::NewLine();
            }
            ImGui::Separator();
        }

        // ---- In-editor C++ script authoring --------------------------------
        static std::string SanitizeIdent(const std::string& in)
        {
            std::string out;
            for (char c : in)
                if (std::isalnum((unsigned char)c) || c == '_')
                    out += c;
            if (!out.empty() && std::isdigit((unsigned char)out[0]))
                out = "_" + out;
            return out;
        }

        static std::string ScriptPath(const std::string& name) { return "Scripts/" + name + ".cpp"; }

        void SaveScript()
        {
            const std::string cls = SanitizeIdent(m_ScriptName);
            if (cls.empty())
            {
                m_ScriptStatus = "Name must be a valid C++ identifier.";
                return;
            }
#ifdef KE_HAS_FILESYSTEM
            std::error_code ec;
            std::filesystem::create_directories("Scripts", ec);
#endif
            const std::string path = ScriptPath(cls);
            std::ofstream out(path, std::ios::out | std::ios::trunc);
            if (!out.is_open())
            {
                m_ScriptStatus = "Could not write " + path + " (read-only filesystem?).";
                return;
            }
            out << m_ScriptBuf.data(); // editor buffer is NUL-terminated
            out.close();
            if (std::find(m_ScriptSaved.begin(), m_ScriptSaved.end(), cls) == m_ScriptSaved.end())
                m_ScriptSaved.push_back(cls);
            m_ScriptStatus = "Saved " + path + " - rebuild (CMake) to compile it in.";
        }

        void LoadScript()
        {
            const std::string cls = SanitizeIdent(m_ScriptName);
            const std::string path = ScriptPath(cls);
            std::ifstream in(path, std::ios::in);
            if (!in.is_open())
            {
                m_ScriptStatus = "No file at " + path;
                return;
            }
            std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            m_ScriptBuf.assign(s.begin(), s.end());
            m_ScriptBuf.resize(std::max<std::size_t>(1u << 15, m_ScriptBuf.size() + 1), '\0');
            m_ScriptStatus = "Loaded " + path;
        }

        void DrawScriptEditor()
        {
            if (m_ScriptBuf.empty())
            {
                const std::string tpl = ScriptTemplate("MyBehaviour");
                m_ScriptBuf.assign(tpl.begin(), tpl.end());
                m_ScriptBuf.resize(1u << 15, '\0');
            }

            ImGui::Begin("Script Editor", &m_ShowScriptEditor);

            ImGui::TextWrapped(
                "Author a C++ behaviour, Save it into Scripts/, then rebuild to compile it in. "
                "It registers by class name (KE_REGISTER_SCRIPT) and becomes selectable in any "
                "entity's Script component - the same code runs on native and web. The browser "
                "build cannot compile at runtime, so saved scripts are picked up on the next "
                "CMake build; native desktop is the live author-and-rebuild loop.");
            ImGui::Separator();

            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText("Class / file", m_ScriptName, sizeof(m_ScriptName));
            ImGui::SameLine();
            if (ImGui::Button("New from template"))
            {
                const std::string tpl = ScriptTemplate(SanitizeIdent(m_ScriptName).empty() ? "MyBehaviour"
                                                                                           : SanitizeIdent(m_ScriptName));
                m_ScriptBuf.assign(tpl.begin(), tpl.end());
                m_ScriptBuf.resize(1u << 15, '\0');
                m_ScriptStatus.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save to Scripts/")) SaveScript();
            ImGui::SameLine();
            if (ImGui::Button("Load")) LoadScript();

            if (!m_ScriptStatus.empty())
                ImGui::TextDisabled("%s", m_ScriptStatus.c_str());
            if (!m_ScriptSaved.empty())
            {
                std::string list = "Saved this session:";
                for (const std::string& s : m_ScriptSaved) list += " " + s;
                ImGui::TextDisabled("%s", list.c_str());
            }

            ImGui::InputTextMultiline("##code", m_ScriptBuf.data(), m_ScriptBuf.size(),
                                      ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_AllowTabInput);
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
