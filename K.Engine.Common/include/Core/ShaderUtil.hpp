#pragma once
#include <Platform/Platform.hpp>
#include <string>
#include <sstream>

namespace KDot
{
    // The shaders are authored as GLSL ES 3.00 (#version 300 es) for WebGL2. On
    // desktop GL we run a 3.3 core context, which rejects the "es" version and
    // the precision qualifiers. This rewrites the source on the fly so a single
    // set of shader files works on both: swap the #version line and drop any
    // top-level `precision` statements (no-ops on desktop anyway).
    inline std::string AdaptShaderForPlatform(const std::string& src)
    {
#if defined(KE_PLATFORM_WEB)
        return src;
#else
        std::istringstream in(src);
        std::ostringstream out;
        std::string line;
        bool versionReplaced = false;

        while (std::getline(in, line))
        {
            const std::size_t firstNonWs = line.find_first_not_of(" \t");
            const std::string trimmed = (firstNonWs == std::string::npos) ? "" : line.substr(firstNonWs);

            if (!versionReplaced && trimmed.rfind("#version", 0) == 0)
            {
                out << "#version 330 core\n";
                versionReplaced = true;
                continue;
            }
            if (trimmed.rfind("precision ", 0) == 0)
                continue; // strip ES precision statements

            out << line << '\n';
        }
        return out.str();
#endif
    }
}
