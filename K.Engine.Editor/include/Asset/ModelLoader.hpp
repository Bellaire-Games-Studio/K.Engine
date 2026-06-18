#pragma once
#include <Core/MeshData.hpp>
#include <string>

namespace KDot
{
    // Imports 3D models into the engine's backend-agnostic MeshData. OBJ is
    // implemented (text, no dependencies, works on web + native); glTF is the
    // planned next format and would slot in as LoadGLTF here.
    namespace ModelLoader
    {
        // Parse a Wavefront OBJ from text / file into 'out' (triangulated,
        // de-duplicated; normals are recomputed if the file has none). Returns
        // false if no geometry was produced.
        bool LoadOBJ(const std::string& text, MeshData& out);
        bool LoadOBJFile(const std::string& path, MeshData& out);
    }
}
