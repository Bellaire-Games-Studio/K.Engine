#pragma once
#include <string>
#include <vector>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <Core/GrassInstance.hpp>
#include <Light.hpp>
#include <Platform/GL.hpp>

namespace KDot
{
    // GPU-instanced grass. The CPU uploads one blade mesh + a per-blade instance
    // buffer once; every frame is a single glDrawArraysInstanced call, with the
    // vertex shader doing the billboarding, taper, wind and distance fade. This
    // keeps the per-frame CPU cost flat regardless of blade count.
    class GrassRenderer
    {
    public:
        GrassRenderer() = default;
        ~GrassRenderer();

        bool Init();
        void SetInstances(const std::vector<GrassInstance>& instances);
        void Render(const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& cameraPos, float time, const LightManager& lights);

        int InstanceCount() const { return m_Count; }

        float bladeWidth  = 0.16f;
        float bladeHeight = 1.6f;
        float maxDistance = 320.0f; // blades fade out past here

    private:
        GLuint CompileShader(const std::string& path, GLenum type);

        GLuint m_Program = 0;
        GLuint m_VAO = 0;
        GLuint m_BladeVBO = 0;
        GLuint m_InstanceVBO = 0;
        int    m_Count = 0;

        GLint u_proj = -1, u_view = -1, u_cam = -1, u_time = -1;
        GLint u_bw = -1, u_bh = -1, u_maxd = -1;
        GLint u_ambC = -1, u_ambI = -1, u_sunD = -1, u_sunC = -1, u_sunI = -1, u_fogC = -1, u_fogD = -1;
    };
}
