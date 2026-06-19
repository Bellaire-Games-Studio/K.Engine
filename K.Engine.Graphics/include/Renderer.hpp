#pragma once
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <Math/Vector.hpp>
#include <Core/MeshData.hpp>
#include <RHI/Device.hpp>
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
        // Tonemap + sRGB resolve: the scene is rendered into an HDR (float) target,
        // then this maps it down to the 8-bit texture the editor displays. Call
        // once, after the scene + HUD have been drawn into the framebuffer.
        void ResolveToneMap();
        bool HdrEnabled() const { return m_HdrEnabled; }
        // Post controls (read by the resolve pass; editable from the inspector).
        float tonemapExposure = 1.0f;
        int   tonemapMode = 1; // 0 None(linear) · 1 ACES · 2 Reinhard · 3 Filmic(Hejl)
        bool  bloomEnabled = true;
        float bloomThreshold = 1.0f; // luminance above which pixels bloom (HDR: >1 = overbright)
        float bloomIntensity = 0.6f;
        // SSAO (screen-space ambient occlusion / contact shadows).
        bool  ssaoEnabled = true;
        float ssaoRadius = 1.5f;    // view-space sampling radius
        float ssaoBias = 0.03f;     // depth bias to avoid self-occlusion
        float ssaoIntensity = 1.0f; // 0 = none

        // ---- Cascaded shadow maps (sun) ------------------------------------
        // WorldLayer computes the per-cascade light matrices each frame and hands
        // them in via SetShadows; the shadow atlas is rendered with the Begin/
        // ShadowCascade/Draw*/End calls below.
        struct ShadowData
        {
            int       count = 0;
            glm::mat4 lightVP[4];
            float     splitFar[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        };
        bool  shadowsEnabled = true;
        float shadowBias = 0.0025f;
        float shadowDistance = 500.0f; // how far from the camera shadows are cast
        int   shadowCascades = 3;
        bool  ShadowsReady() const { return m_ShadowReady; }
        int   ShadowMapSize() const { return m_ShadowSize; }

        void InitShadows();                                        // atlas + depth programs + cube VBO
        void BeginShadowPass();                                    // bind atlas, clear, set state
        void ShadowCascade(int index, const glm::mat4 &lightVP);   // viewport + active light matrix
        void DrawTerrainShadow();                                  // depth-render the cached terrain meshes
        void DrawCubeShadow(const glm::mat4 &model);               // depth-render one box caster
        void EndShadowPass();
        void SetShadows(const ShadowData &sd);                     // upload to the main program + bind atlas
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
        // The editor displays the tonemapped LDR result (falls back to the raw
        // scene texture if the tonemap program failed to build).
        GLuint GetFrameBufferTexture() { return m_TonemapProgram ? m_ResolveTexture : m_Texture; }
        void LoadTexture(const char *path);
        // Load an image file as a sampled texture; returns a 1-based slot to put on
        // a Prop (0 on failure). CreateCheckerTexture makes a built-in checkerboard
        // (works with no asset files, e.g. in the browser build).
        int LoadTextureFile(const char *path);
        int CreateCheckerTexture();
        int TextureCount() const { return (int)m_Textures.size(); }
        const char *TexturePath(int slot) const
        {
            return (slot >= 1 && slot <= (int)m_TexturePaths.size()) ? m_TexturePaths[slot - 1].c_str() : "";
        }
        // Draw an arbitrary indexed mesh (e.g. a terrain chunk) with the active
        // camera. Uploaded immediately and drawn in its own draw call; this is
        // separate from the quad/cube batch above. Call between BeginStream and
        // EndStream so the view/projection match the rest of the frame.
        void DrawMesh(const MeshData &mesh);
        // Cached variant: keeps a GPU buffer per cacheKey and only re-uploads when
        // 'version' changes (see TerrainChunk::MeshVersion). Used for terrain so
        // static chunks aren't re-uploaded every frame.
        void DrawMesh(const MeshData &mesh, std::uint32_t cacheKey, std::uint32_t version);
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
        void ConfigureMeshVertexAttribs(); // MeshVertex attribute layout for a bound VAO/VBO
        void BindTextures();               // bind loaded Prop textures to units 0..N
        // Prop textures (1-based slots; bound to texture units for the main shader).
        std::vector<GLuint>      m_Textures;
        std::vector<std::string> m_TexturePaths;
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
        // Per-point-light uniform locations, cached once at compile time instead
        // of being re-queried by string every frame (one of the per-frame costs).
        GLint u_PointPos[kMaxShaderPointLights];
        GLint u_PointColor[kMaxShaderPointLights];
        GLint u_PointIntensity[kMaxShaderPointLights];
        GLint u_PointRadius[kMaxShaderPointLights];
        glm::mat4 m_ActiveProjection = glm::mat4(1.0f);
        glm::mat4 m_SceneProjection = glm::mat4(1.0f); // perspective from BeginStream (survives Begin2D)
        int       m_ShadeMode = 0;
        glm::vec3 m_CameraWorldPos = glm::vec3(0.0f);
        GLuint m_ShaderProgram;
        GLuint m_FrameBuffer;
        GLuint m_VertexArray;
        GLuint m_VertexBuffer;
        GLuint m_RenderBuffer;
        GLuint m_Texture; // scene colour attachment (HDR RGBA16F when supported)
        GLuint m_DepthTex = 0; // scene depth, as a sampleable texture (for SSAO)
        // HDR -> LDR tonemap resolve.
        GLuint m_ResolveFBO = 0;
        GLuint m_ResolveTexture = 0;
        GLuint m_TonemapProgram = 0;
        GLuint m_TonemapVAO = 0;
        GLint  u_TmHdr = -1;
        GLint  u_TmExposure = -1;
        GLint  u_TmMode = -1;
        GLint  u_TmBloom = -1;
        GLint  u_TmBloomIntensity = -1;
        bool   m_HdrEnabled = false;
        void   BuildTonemapResources(int width, int height);
        // RHI device used by the passes that have been ported off raw GL (bloom
        // today; the rest of the renderer migrates onto it over time).
        std::unique_ptr<rhi::Device> m_Rhi;
        // Bloom (bright-pass + separable blur on a half-res HDR chain) - ported to
        // the RHI. m_BloomSceneTex borrows the scene colour texture (m_Texture)
        // until the scene target is itself moved onto the RHI.
        int    m_BloomW = 0, m_BloomH = 0;
        std::unique_ptr<rhi::Texture>      m_BloomSceneTex;
        std::unique_ptr<rhi::Texture>      m_BrightTex;
        std::unique_ptr<rhi::Texture>      m_BlurTex[2];
        std::unique_ptr<rhi::RenderTarget> m_BrightRT;
        std::unique_ptr<rhi::RenderTarget> m_BlurRT[2];
        std::unique_ptr<rhi::Pipeline>     m_BrightPipe;
        std::unique_ptr<rhi::Pipeline>     m_BlurPipe;
        std::unique_ptr<rhi::Buffer>       m_PostUbo; // per-pass "Post" constants
        GLint  u_TmAo = -1, u_TmAoEnabled = -1;
        void   BuildBloomResources(int width, int height);
        GLuint RenderBloom(); // returns the GL id of the final blurred bloom texture

        // SSAO: half-res occlusion from the scene depth texture, then a box blur -
        // ported to the RHI. m_SsaoDepthTex borrows the GL scene depth texture
        // (m_DepthTex) until the scene target moves onto the RHI.
        int    m_SsaoW = 0, m_SsaoH = 0;
        std::unique_ptr<rhi::Texture>      m_SsaoDepthTex;
        std::unique_ptr<rhi::Texture>      m_SsaoTex;
        std::unique_ptr<rhi::Texture>      m_SsaoBlurTex;
        std::unique_ptr<rhi::RenderTarget> m_SsaoRT;
        std::unique_ptr<rhi::RenderTarget> m_SsaoBlurRT;
        std::unique_ptr<rhi::Pipeline>     m_SsaoPipe;
        std::unique_ptr<rhi::Pipeline>     m_SsaoBlurPipe;
        std::unique_ptr<rhi::Buffer>       m_SsaoUbo; // "Ssao" block (proj/invProj/params)
        void   BuildSsaoResources(int width, int height);
        GLuint RenderSSAO(); // returns the GL id of the blurred AO texture (0 if off)

        // Shadow atlas (cascades tiled horizontally) + depth-only programs.
        static constexpr int kShadowUnit = 15; // texture unit reserved for the shadow atlas
        bool   m_ShadowReady = false;
        int    m_ShadowSize = 0;  // per-cascade resolution (atlas width = count * size)
        int    m_ShadowCount = 0; // cascades the atlas was built for
        GLuint m_ShadowFBO = 0, m_ShadowTex = 0;
        GLuint m_DepthWorldProg = 0; // world-space casters (terrain)
        GLuint m_DepthModelProg = 0; // unit cube + model (props)
        GLint  u_DwLightVP = -1, u_DmLightVP = -1, u_DmModel = -1;
        GLuint m_CubeVAO = 0, m_CubeVBO = 0, m_CubeIBO = 0;
        glm::mat4 m_CurShadowVP = glm::mat4(1.0f);
        GLint  u_ShadowCount = -1, u_ShadowVP = -1, u_ShadowSplit = -1;
        GLint  u_ShadowAtlas = -1, u_ShadowBias = -1, u_ShadowTexel = -1;
        // Dedicated buffers for DrawMesh (terrain / arbitrary indexed geometry).
        GLuint m_MeshVAO = 0;
        GLuint m_MeshVBO = 0;
        GLuint m_MeshIBO = 0;
        bool   m_MeshBuffersReady = false;
        // Per-key cached mesh GPU buffers (terrain chunks): upload once, re-upload
        // only when the source mesh version changes.
        struct MeshCacheEntry
        {
            GLuint        vao = 0;
            GLuint        vbo = 0;
            GLuint        ibo = 0;
            GLsizei       indexCount = 0;
            std::uint32_t version = 0xFFFFFFFFu; // sentinel: never uploaded
        };
        std::unordered_map<std::uint32_t, MeshCacheEntry> m_MeshCache;
        glm::mat4 viewMatrix;
        float cameraZoom = 90.0f;
        float m_Height = 1280.0f;
        float m_Width = 720.0f;
        char *m_VertexShader;
        char *m_FragmentShader;
        bool CreateProgram();
    };

} // namespace KDot
