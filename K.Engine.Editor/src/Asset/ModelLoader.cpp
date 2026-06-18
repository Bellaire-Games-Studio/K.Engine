#include <Asset/ModelLoader.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstdlib>

namespace KDot
{
    namespace
    {
        // Resolve a 1-based OBJ index (negative = relative to the end).
        int Resolve(int idx, std::size_t count)
        {
            if (idx > 0) return idx - 1;
            if (idx < 0) return (int)count + idx;
            return -1;
        }
    }

    bool ModelLoader::LoadOBJ(const std::string& text, MeshData& out)
    {
        out.Clear();

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::unordered_map<std::string, std::uint32_t> cache; // "v/vt/vn" -> out index
        bool sawNormals = false;

        // Emit (or reuse) a vertex for one face corner token like "3/1/2".
        auto emit = [&](const std::string& tok) {
            auto it = cache.find(tok);
            if (it != cache.end())
            {
                out.indices.push_back(it->second);
                return;
            }

            int vi = 0, ti = 0, ni = 0;
            // split on '/', allowing "v", "v/t", "v//n", "v/t/n"
            std::size_t s1 = tok.find('/');
            if (s1 == std::string::npos)
            {
                vi = std::atoi(tok.c_str());
            }
            else
            {
                vi = std::atoi(tok.substr(0, s1).c_str());
                std::size_t s2 = tok.find('/', s1 + 1);
                if (s2 == std::string::npos)
                {
                    ti = std::atoi(tok.substr(s1 + 1).c_str());
                }
                else
                {
                    if (s2 > s1 + 1)
                        ti = std::atoi(tok.substr(s1 + 1, s2 - s1 - 1).c_str());
                    ni = std::atoi(tok.substr(s2 + 1).c_str());
                }
            }

            MeshVertex v;
            const int pi = Resolve(vi, positions.size());
            if (pi >= 0 && pi < (int)positions.size())
                v.position = positions[pi];
            const int tti = Resolve(ti, uvs.size());
            if (tti >= 0 && tti < (int)uvs.size())
                v.texCoord = uvs[tti];
            const int nni = Resolve(ni, normals.size());
            if (nni >= 0 && nni < (int)normals.size())
            {
                v.normal = normals[nni];
                sawNormals = true;
            }

            const std::uint32_t index = (std::uint32_t)out.vertices.size();
            out.vertices.push_back(v);
            cache.emplace(tok, index);
            out.indices.push_back(index);
        };

        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            std::istringstream ls(line);
            std::string key;
            if (!(ls >> key))
                continue;

            if (key == "v")
            {
                glm::vec3 p(0.0f);
                ls >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (key == "vt")
            {
                glm::vec2 t(0.0f);
                ls >> t.x >> t.y;
                uvs.push_back(t);
            }
            else if (key == "vn")
            {
                glm::vec3 n(0.0f);
                ls >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (key == "f")
            {
                std::vector<std::string> face;
                std::string tok;
                while (ls >> tok)
                    face.push_back(tok);
                // Triangulate the polygon as a fan.
                for (std::size_t i = 1; i + 1 < face.size(); ++i)
                {
                    emit(face[0]);
                    emit(face[i]);
                    emit(face[i + 1]);
                }
            }
        }

        if (!sawNormals)
            out.RecalculateNormals();

        return !out.indices.empty();
    }

    bool ModelLoader::LoadOBJFile(const std::string& path, MeshData& out)
    {
        std::ifstream f(path, std::ios::in);
        if (!f.is_open())
            return false;
        std::stringstream ss;
        ss << f.rdbuf();
        return LoadOBJ(ss.str(), out);
    }
}
