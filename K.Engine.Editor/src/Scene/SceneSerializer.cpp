#include <Scene/SceneSerializer.hpp>
#include <Core/Transform.hpp>
#include <Core/SceneComponents.hpp>
#include <Script/ScriptBehavior.hpp>
#include <Rigidbody.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace KDot
{
    namespace
    {
        void WriteVec3(std::ostream& o, const glm::vec3& v) { o << v.x << ' ' << v.y << ' ' << v.z; }

        // Reads "key rest-of-line" where the value is free text (entity names).
        std::string Trim(const std::string& s)
        {
            std::size_t a = s.find_first_not_of(" \t\r\n");
            std::size_t b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        }
    }

    std::string SceneSerializer::SaveToString(ecs::Registry& reg, const SceneEnv& env)
    {
        std::ostringstream o;
        o << "kscene 1\n";

        o << "env\n";
        o << "sun_dir ";          WriteVec3(o, env.sunDir);       o << '\n';
        o << "sun_color ";        WriteVec3(o, env.sunColor);     o << '\n';
        o << "sun_intensity " << env.sunIntensity << '\n';
        o << "ambient_color ";    WriteVec3(o, env.ambientColor); o << '\n';
        o << "ambient_intensity " << env.ambientIntensity << '\n';
        o << "fog_color ";        WriteVec3(o, env.fogColor);     o << '\n';
        o << "fog_density " << env.fogDensity << '\n';
        o << "terrain_seed " << env.terrainSeed << '\n';
        o << "grass_show " << (env.grassShow ? 1 : 0) << '\n';
        o << "grass_distance " << env.grassDistance << '\n';
        o << "end\n";

        // Serialize every named entity, in stable index order.
        std::vector<ecs::Entity> ents;
        reg.View<Name>([&](ecs::Entity e, Name&) { ents.push_back(e); });
        std::sort(ents.begin(), ents.end(),
                  [](ecs::Entity a, ecs::Entity b) { return ecs::IndexOf(a) < ecs::IndexOf(b); });

        for (ecs::Entity e : ents)
        {
            o << "entity\n";
            if (Name* n = reg.TryGet<Name>(e))
                o << "name " << n->value << '\n';
            if (Transform* t = reg.TryGet<Transform>(e))
            {
                o << "transform ";
                WriteVec3(o, t->position); o << ' ';
                o << t->rotation.x << ' ' << t->rotation.y << ' ' << t->rotation.z << ' ' << t->rotation.w << ' ';
                WriteVec3(o, t->scale); o << '\n';
            }
            if (Prop* p = reg.TryGet<Prop>(e))
            {
                o << "prop "; WriteVec3(o, p->size);
                o << ' ' << p->color.r << ' ' << p->color.g << ' ' << p->color.b << ' ' << p->color.a << '\n';
            }
            if (LightSource* l = reg.TryGet<LightSource>(e))
            {
                o << "light "; WriteVec3(o, l->color);
                o << ' ' << l->intensity << ' ' << l->radius << '\n';
            }
            if (Rigidbody* rb = reg.TryGet<Rigidbody>(e))
            {
                o << "rigidbody " << rb->mass << ' ' << rb->restitution << ' ' << rb->friction << ' '
                  << rb->linearDamping << ' ' << (rb->useGravity ? 1 : 0) << ' ' << (rb->isStatic ? 1 : 0) << '\n';
            }
            if (Collider* c = reg.TryGet<Collider>(e))
            {
                o << "collider " << (int)c->type << ' ';
                WriteVec3(o, c->halfExtents); o << ' ' << c->radius << ' ';
                WriteVec3(o, c->localOffset); o << ' ' << (c->isTrigger ? 1 : 0) << '\n';
            }
            if (Script* s = reg.TryGet<Script>(e))
                o << "script " << s->name << '\n';
            o << "end\n";
        }
        return o.str();
    }

    bool SceneSerializer::LoadFromString(const std::string& text, ecs::Registry& reg, SceneEnv& env)
    {
        std::istringstream in(text);
        std::string line;

        if (!std::getline(in, line) || Trim(line).rfind("kscene", 0) != 0)
            return false;

        reg = ecs::Registry{}; // clear existing world
        env = SceneEnv{};

        ecs::Entity cur = ecs::kNull;

        while (std::getline(in, line))
        {
            std::istringstream ls(line);
            std::string key;
            if (!(ls >> key))
                continue;

            if (key == "env" || key == "end")
            {
                cur = ecs::kNull;
            }
            else if (key == "entity")
            {
                cur = reg.Create();
            }
            // ---- environment ----
            else if (key == "sun_dir")            ls >> env.sunDir.x >> env.sunDir.y >> env.sunDir.z;
            else if (key == "sun_color")          ls >> env.sunColor.x >> env.sunColor.y >> env.sunColor.z;
            else if (key == "sun_intensity")      ls >> env.sunIntensity;
            else if (key == "ambient_color")      ls >> env.ambientColor.x >> env.ambientColor.y >> env.ambientColor.z;
            else if (key == "ambient_intensity")  ls >> env.ambientIntensity;
            else if (key == "fog_color")          ls >> env.fogColor.x >> env.fogColor.y >> env.fogColor.z;
            else if (key == "fog_density")        ls >> env.fogDensity;
            else if (key == "terrain_seed")       ls >> env.terrainSeed;
            else if (key == "grass_show")         { int v = 1; ls >> v; env.grassShow = v != 0; }
            else if (key == "grass_distance")     ls >> env.grassDistance;
            // ---- entity components ----
            else if (cur != ecs::kNull && key == "name")
            {
                std::string rest;
                std::getline(ls, rest);
                reg.Emplace<Name>(cur, Name{Trim(rest)});
            }
            else if (cur != ecs::kNull && key == "transform")
            {
                Transform t;
                ls >> t.position.x >> t.position.y >> t.position.z
                   >> t.rotation.x >> t.rotation.y >> t.rotation.z >> t.rotation.w
                   >> t.scale.x >> t.scale.y >> t.scale.z;
                reg.Emplace<Transform>(cur, t);
            }
            else if (cur != ecs::kNull && key == "prop")
            {
                Prop p;
                ls >> p.size.x >> p.size.y >> p.size.z >> p.color.r >> p.color.g >> p.color.b >> p.color.a;
                reg.Emplace<Prop>(cur, p);
            }
            else if (cur != ecs::kNull && key == "light")
            {
                LightSource l;
                ls >> l.color.r >> l.color.g >> l.color.b >> l.intensity >> l.radius;
                reg.Emplace<LightSource>(cur, l);
            }
            else if (cur != ecs::kNull && key == "rigidbody")
            {
                Rigidbody rb;
                float mass = 1.0f;
                int useG = 1, isStat = 0;
                ls >> mass >> rb.restitution >> rb.friction >> rb.linearDamping >> useG >> isStat;
                rb.useGravity = useG != 0;
                rb.SetMass(mass);
                rb.SetStatic(isStat != 0);
                reg.Emplace<Rigidbody>(cur, rb);
            }
            else if (cur != ecs::kNull && key == "collider")
            {
                Collider c;
                int type = 0, trig = 0;
                ls >> type >> c.halfExtents.x >> c.halfExtents.y >> c.halfExtents.z >> c.radius
                   >> c.localOffset.x >> c.localOffset.y >> c.localOffset.z >> trig;
                c.type = (ColliderType)type;
                c.isTrigger = trig != 0;
                reg.Emplace<Collider>(cur, c);
            }
            else if (cur != ecs::kNull && key == "script")
            {
                std::string rest;
                std::getline(ls, rest);
                reg.Emplace<Script>(cur).name = Trim(rest);
            }
        }
        return true;
    }

    bool SceneSerializer::Save(const std::string& path, ecs::Registry& reg, const SceneEnv& env)
    {
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out.is_open())
            return false;
        out << SaveToString(reg, env);
        return true;
    }

    bool SceneSerializer::Load(const std::string& path, ecs::Registry& reg, SceneEnv& env)
    {
        std::ifstream in(path, std::ios::in);
        if (!in.is_open())
            return false;
        std::stringstream ss;
        ss << in.rdbuf();
        return LoadFromString(ss.str(), reg, env);
    }
}
