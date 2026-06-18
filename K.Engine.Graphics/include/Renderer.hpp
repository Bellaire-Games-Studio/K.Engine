#pragma once
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <Math/Vector.hpp>
#include <Core/MeshData.hpp>
#include <Light.hpp>
#include <glm.hpp>
#include <gtx/rotate_vector.hpp>
#include <gtx/transform.hpp>
#include <ext.hpp>
#include <Camera.hpp>
#include <Platform/GL.hpp>
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
        // Orthographic 2D pass for HUD / sprites. Coordinates are in pixels with
        // (0,0) at the top-left of the given virtual size. Lighting + depth test
        // are disabled and alpha blending enabled for the duration.
        void Begin2D(float width, float height);
        void End2D();
        // Upload scene lighting (point lights are culled to the nearest few,
        // capped by QualitySettings). Call once per frame inside BeginStream.
        void SetLights(const LightManager &lights, const glm::vec3 &cameraPos);
        GLuint GetFrameBufferTexture() { return m_Texture; }
        void LoadTexture(const char *path);
        // Draw an arbitrary indexed mesh (e.g. a terrain chunk) with the active
        // camera. Uploaded immediately and drawn in its own draw call; this is
        // separate from the quad/cube batch above. Call between BeginStream and
        // EndStream so the view/projection match the rest of the frame.
        void DrawMesh(const MeshData &mesh);
        void DrawCube(const glm::vec3 &position, const glm::vec3 &size, const glm::vec4 &color, float angle, unsigned int textureIndex = 0);
        // Draw a unit cube transformed by an arbitrary matrix (hierarchical world
        // transforms, gizmos). Normals are carried through the matrix.
        void DrawCube(const glm::mat4 &transform, const glm::vec4 &color, unsigned int textureIndex = 0);
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
        void InitMeshBuffers();
        void ApplyFrameUniforms(); // binds program + sets projection/view/unlit/camera
        int m_CurrentTextureIndex = 0;
        glm::vec4 m_QuadVertexPositions[4];
        glm::vec4 m_CubeVertexPositions[8];
        glm::vec4 m_CubeVertexNormals[8];
        glm::vec2 m_CubeVertexTexCoords[4];
        uint32_t *QuadIndices;

        void StartBatch();
        void ResetBatch();
        void NextBatch();
        void BindVertexBuffer();
        void UnbindVertexBuffer();
        void BindVertexArray();
        void UnbindVertexArray();
        const uint32_t MaxCubes = 5000;
        const uint32_t MaxQuads = MaxCubes * 6;
        const uint32_t MaxVertices = MaxQuads * 4;
        const uint32_t MaxIndices = MaxQuads * 6;
        uint32_t m_QuadIndexCount = 0;
        Vertex *m_QuadVertexBufferBase = nullptr;
        Vertex *m_QuadVertexBufferPtr = nullptr;

        static constexpr int kMaxShaderPointLights = 16;
        GLint u_ProjectionMat;
        GLint u_ViewMat;
        GLint u_ModelMat;
        // Lighting / mode uniform locations (cached at compile time).
        GLint u_ShadeMode = -1; // 0=lit vertex colour, 1=procedural terrain, 2=unlit 2D
        GLint u_CameraPos = -1;
        GLint u_AmbientColor = -1;
        GLint u_AmbientIntensity = -1;
        GLint u_SunDir = -1;
        GLint u_SunColor = -1;
        GLint u_SunIntensity = -1;
        GLint u_PointCount = -1;
        GLint u_FogColor = -1;
        GLint u_FogDensity = -1;
        glm::mat4 m_ActiveProjection = glm::mat4(1.0f);
        int       m_ShadeMode = 0;
        glm::vec3 m_CameraWorldPos = glm::vec3(0.0f);
        GLuint m_ShaderProgram;
        GLuint m_FrameBuffer;
        GLuint m_VertexArray;
        GLuint m_VertexBuffer;
        GLuint m_RenderBuffer;
        GLuint m_Texture;
        // Dedicated buffers for DrawMesh (terrain / arbitrary indexed geometry).
        GLuint m_MeshVAO = 0;
        GLuint m_MeshVBO = 0;
        GLuint m_MeshIBO = 0;
        bool   m_MeshBuffersReady = false;
        glm::mat4 viewMatrix;
        float cameraZoom = 90.0f;
        float m_Height = 1280.0f;
        float m_Width = 720.0f;
        char *m_VertexShader;
        char *m_FragmentShader;
        bool CreateProgram();
    };

} // namespace KDot
