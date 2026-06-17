#include <GrassRenderer.hpp>
#include <iostream>
#include <fstream>
#include <iterator>
#include <cstddef>

namespace KDot
{
    GrassRenderer::~GrassRenderer()
    {
        if (m_Program) glDeleteProgram(m_Program);
        if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
        if (m_BladeVBO) glDeleteBuffers(1, &m_BladeVBO);
        if (m_InstanceVBO) glDeleteBuffers(1, &m_InstanceVBO);
    }

    GLuint GrassRenderer::CompileShader(const std::string& path, GLenum type)
    {
        std::ifstream in(path, std::ios::in);
        if (!in.is_open())
        {
            std::cout << "Grass: failed to open shader " << path << std::endl;
            return 0;
        }
        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        const char* c = src.c_str();
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &c, NULL);
        glCompileShader(shader);

        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[1024] = {0};
            glGetShaderInfoLog(shader, sizeof(log), NULL, log);
            std::cout << "Grass shader compile error (" << path << "): " << log << std::endl;
        }
        return shader;
    }

    bool GrassRenderer::Init()
    {
        m_Program = glCreateProgram();
        GLuint vs = CompileShader("Assets/Shaders/3D/GrassVertex.glsl", GL_VERTEX_SHADER);
        GLuint fs = CompileShader("Assets/Shaders/3D/GrassFragment.glsl", GL_FRAGMENT_SHADER);
        if (!vs || !fs)
            return false;

        glAttachShader(m_Program, vs);
        glAttachShader(m_Program, fs);
        glLinkProgram(m_Program);

        GLint linked = 0;
        glGetProgramiv(m_Program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            char log[1024] = {0};
            glGetProgramInfoLog(m_Program, sizeof(log), NULL, log);
            std::cout << "Grass program link error: " << log << std::endl;
            return false;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);

        // One tapered blade quad (2 triangles): vBlade = (xSign, heightT).
        static const float blade[12] = {
            -1.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            -1.0f, 0.0f,  1.0f, 1.0f, -1.0f, 1.0f};

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_BladeVBO);
        glGenBuffers(1, &m_InstanceVBO);

        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_BladeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(blade), blade, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GrassInstance), (void*)offsetof(GrassInstance, position));
        glVertexAttribDivisor(1, 1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GrassInstance), (void*)offsetof(GrassInstance, phase));
        glVertexAttribDivisor(2, 1);

        glBindVertexArray(0);

        u_proj = glGetUniformLocation(m_Program, "projection");
        u_view = glGetUniformLocation(m_Program, "view");
        u_cam  = glGetUniformLocation(m_Program, "uCameraPos");
        u_time = glGetUniformLocation(m_Program, "uTime");
        u_bw   = glGetUniformLocation(m_Program, "uBladeWidth");
        u_bh   = glGetUniformLocation(m_Program, "uBladeHeight");
        u_maxd = glGetUniformLocation(m_Program, "uMaxDist");
        u_ambC = glGetUniformLocation(m_Program, "uAmbientColor");
        u_ambI = glGetUniformLocation(m_Program, "uAmbientIntensity");
        u_sunD = glGetUniformLocation(m_Program, "uSunDir");
        u_sunC = glGetUniformLocation(m_Program, "uSunColor");
        u_sunI = glGetUniformLocation(m_Program, "uSunIntensity");
        u_fogC = glGetUniformLocation(m_Program, "uFogColor");
        u_fogD = glGetUniformLocation(m_Program, "uFogDensity");
        return true;
    }

    void GrassRenderer::SetInstances(const std::vector<GrassInstance>& instances)
    {
        m_Count = static_cast<int>(instances.size());
        if (!m_InstanceVBO)
            return;
        glBindBuffer(GL_ARRAY_BUFFER, m_InstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(GrassInstance),
                     instances.empty() ? nullptr : instances.data(), GL_STATIC_DRAW);
    }

    void GrassRenderer::Render(const glm::mat4& view, const glm::mat4& proj,
                               const glm::vec3& cameraPos, float time, const LightManager& lights)
    {
        if (m_Count <= 0 || !m_Program)
            return;

        glUseProgram(m_Program);

        glUniformMatrix4fv(u_proj, 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(u_view, 1, GL_FALSE, &view[0][0]);
        glUniform3fv(u_cam, 1, &cameraPos[0]);
        glUniform1f(u_time, time);
        glUniform1f(u_bw, bladeWidth);
        glUniform1f(u_bh, bladeHeight);
        glUniform1f(u_maxd, maxDistance);

        glUniform3fv(u_ambC, 1, &lights.ambient.color[0]);
        glUniform1f(u_ambI, lights.ambient.intensity);
        glm::vec3 sunDir = glm::normalize(lights.sun.direction);
        glUniform3fv(u_sunD, 1, &sunDir[0]);
        glUniform3fv(u_sunC, 1, &lights.sun.color[0]);
        glUniform1f(u_sunI, lights.sun.intensity);
        glUniform3fv(u_fogC, 1, &lights.fogColor[0]);
        glUniform1f(u_fogD, lights.fogDensity);

        glBindVertexArray(m_VAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, m_Count);
        glBindVertexArray(0);
    }
}
