#pragma once
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <Math/Vector.hpp>
#include <glm.hpp>
#include <gtx/rotate_vector.hpp>
#include <gtx/transform.hpp>
#include <ext.hpp>
#include <Camera.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif
namespace KDot
{
    float closestResolution(int n, int multiple);
    struct Vertex
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec3 normal;
        glm::vec2 texCoord;
        int texIndex; // Corresponds to the texture index in the shader
    };
    
    class Renderer
    {
    public:
        Renderer() = default;
        virtual ~Renderer();
        void RescaleFrameBuffer(int width, int height);
        bool InitShader(const char *Shader, GLenum type);
        bool InitShader(const char **Shader, GLenum type);
        bool InitShaderFromFile(const char *path, GLenum type);
        void Flush();
        bool Compile(); // Create and compile shaders
        void GenerateFrameBuffer();
        void BindFrameBuffer();
        void UnbindFrameBuffer();
        void BeginStream(Camera &camera);
        void EndStream();
        GLuint GetFrameBufferTexture() { return m_Texture; }
        void LoadTexture(const char *path);
        void DrawCube(const glm::vec3 &position, const glm::vec3 &size, const glm::vec4 &color, float angle, unsigned int textureIndex = 0);
        void DrawQuad(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color, float rotation, const glm::vec2 &center);
        void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color);
        void DrawQuad(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color);
        void DrawQuad(const glm::mat4 &transform, const glm::vec4 &color);
        int DrawCallCount = 0;
        int QuadCount = 0;
        int Triangles = 0;
        glm::vec3 m_CameraPosition = {0.0f, 0.0f, -5.0f};

    private:
        void InitiateVertexBuffer(float *vertices, GLuint size);
        void InitiateVertexBuffer(GLuint size);
        void InitiateVertexArray();
        void SetVertexBufferData(const void *data, GLuint size);
        void AddVertexBuffer();
        void AddIndexBuffer();
        int m_CurrentTextureIndex = 0;
        glm::vec4 m_QuadVertexPositions[4];
        glm::vec4 m_CubeVertexPositions[8];
        glm::vec4 m_CubeVertexNormals[8];
        glm::vec2 m_CubeVertexTexCoords[4];
        uint32_t *QuadIndices;

        void StartBatch();
        void NextBatch();
        void BindVertexBuffer();
        void UnbindVertexBuffer();
        void BindVertexArray();
        void UnbindVertexArray();
        void BindIndexBuffer();
        void UnbindIndexBuffer();
        const uint32_t MaxCubes = 5000;
        const uint32_t MaxQuads = MaxCubes * 6;
        const uint32_t MaxVertices = MaxQuads * 4;
        const uint32_t MaxIndices = MaxQuads * 6;
        uint32_t m_QuadIndexCount = 0;
        Vertex *m_QuadVertexBufferBase = nullptr;
        Vertex *m_QuadVertexBufferPtr = nullptr;

        GLint u_ProjectionMat;
        GLint u_ViewMat;
        GLint u_ModelMat;
        GLuint m_ShaderProgram;
        GLuint m_FrameBuffer;
        GLuint m_VertexArray;
        GLuint m_VertexBuffer;
        GLuint m_IndexBuffer;
        GLuint m_RenderBuffer;
        GLuint m_Texture;
        glm::mat4 viewMatrix;
        float cameraZoom = 90.0f;
        float m_Height = 1280.0f;
        float m_Width = 720.0f;
        char *m_VertexShader;
        char *m_FragmentShader;
        bool CreateProgram();
    };

} // namespace KDot
