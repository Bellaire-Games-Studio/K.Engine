#include <Renderer.hpp>
#include <Core/QualitySettings.hpp>
#include <Core/ShaderUtil.hpp>
#include <algorithm>
#include <cstddef>
#define STB_IMAGE_IMPLEMENTATION
#include <Image/stb_image.h>
#define GLTextureMapIndex(x) (x == 0 ? GL_TEXTURE0 : x == 1 ? GL_TEXTURE1 : x == 2 ? GL_TEXTURE2 : x == 3 ? GL_TEXTURE3 : x == 4 ? GL_TEXTURE4 : x == 5 ? GL_TEXTURE5 : x == 6 ? GL_TEXTURE6 : x == 7 ? GL_TEXTURE7 : x == 8 ? GL_TEXTURE8 : x == 9 ? GL_TEXTURE9 : x == 10 ? GL_TEXTURE10 : x == 11 ? GL_TEXTURE11 : x == 12 ? GL_TEXTURE12 : x == 13 ? GL_TEXTURE13 : x == 14 ? GL_TEXTURE14 : x == 15 ? GL_TEXTURE15 : x == 16 ? GL_TEXTURE16 : x == 17 ? GL_TEXTURE17 : x == 18 ? GL_TEXTURE18 : x == 19 ? GL_TEXTURE19 : x == 20 ? GL_TEXTURE20 : x == 21 ? GL_TEXTURE21 : x == 22 ? GL_TEXTURE22 : x == 23 ? GL_TEXTURE23 : x == 24 ? GL_TEXTURE24 : x == 25 ? GL_TEXTURE25 : x == 26 ? GL_TEXTURE26 : x == 27 ? GL_TEXTURE27 : x == 28 ? GL_TEXTURE28 : x == 29 ? GL_TEXTURE29 : x == 30 ? GL_TEXTURE30 : x == 31 ? GL_TEXTURE31 : 0)

namespace KDot
{

    Renderer::~Renderer()
    {
        glDeleteProgram(m_ShaderProgram);
        if (m_MeshBuffersReady)
        {
            glDeleteVertexArrays(1, &m_MeshVAO);
            glDeleteBuffers(1, &m_MeshVBO);
            glDeleteBuffers(1, &m_MeshIBO);
        }
        if (m_TonemapProgram) glDeleteProgram(m_TonemapProgram);
        if (m_TonemapVAO)     glDeleteVertexArrays(1, &m_TonemapVAO);
        if (m_ResolveTexture) glDeleteTextures(1, &m_ResolveTexture);
        if (m_ResolveFBO)     glDeleteFramebuffers(1, &m_ResolveFBO);
        if (m_BrightProgram)  glDeleteProgram(m_BrightProgram);
        if (m_BlurProgram)    glDeleteProgram(m_BlurProgram);
        if (m_BrightTex)      glDeleteTextures(1, &m_BrightTex);
        if (m_BrightFBO)      glDeleteFramebuffers(1, &m_BrightFBO);
        for (int i = 0; i < 2; ++i)
        {
            if (m_BlurTex[i]) glDeleteTextures(1, &m_BlurTex[i]);
            if (m_BlurFBO[i]) glDeleteFramebuffers(1, &m_BlurFBO[i]);
        }
        for (auto &kv : m_MeshCache)
        {
            if (kv.second.vao) glDeleteVertexArrays(1, &kv.second.vao);
            if (kv.second.vbo) glDeleteBuffers(1, &kv.second.vbo);
            if (kv.second.ibo) glDeleteBuffers(1, &kv.second.ibo);
        }
    }

    namespace
    {
        // Compile a single shader stage from GLES-3.00 source (adapted to desktop
        // GL on the fly). Returns 0 on failure.
        GLuint CompileTonemapStage(GLenum type, const std::string& src)
        {
            const std::string adapted = AdaptShaderForPlatform(src);
            const char* c = adapted.c_str();
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &c, NULL);
            glCompileShader(sh);
            GLint ok = 0;
            glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char log[1024] = {0};
                glGetShaderInfoLog(sh, sizeof(log), NULL, log);
                std::cout << "Tonemap shader compile error: " << log << std::endl;
                glDeleteShader(sh);
                return 0;
            }
            return sh;
        }

        GLuint BuildStandaloneProgram(const char* vs, const char* fs)
        {
            GLuint v = CompileTonemapStage(GL_VERTEX_SHADER, vs);
            GLuint f = CompileTonemapStage(GL_FRAGMENT_SHADER, fs);
            if (!v || !f)
            {
                if (v) glDeleteShader(v);
                if (f) glDeleteShader(f);
                return 0;
            }
            GLuint p = glCreateProgram();
            glAttachShader(p, v);
            glAttachShader(p, f);
            glLinkProgram(p);
            GLint linked = 0;
            glGetProgramiv(p, GL_LINK_STATUS, &linked);
            glDeleteShader(v);
            glDeleteShader(f);
            if (!linked)
            {
                char log[1024] = {0};
                glGetProgramInfoLog(p, sizeof(log), NULL, log);
                std::cout << "Tonemap program link error: " << log << std::endl;
                glDeleteProgram(p);
                return 0;
            }
            return p;
        }

        // Fullscreen triangle generated from gl_VertexID (no vertex buffer needed).
        const char* kTonemapVert =
            "#version 300 es\n"
            "precision highp float;\n"
            "out vec2 vUV;\n"
            "void main(){\n"
            "    float x = float((gl_VertexID << 1) & 2);\n"
            "    float y = float(gl_VertexID & 2);\n"
            "    vUV = vec2(x, y);\n"
            "    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);\n"
            "}\n";

        const char* kTonemapFrag =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec2 vUV;\n"
            "out vec4 fragColor;\n"
            "uniform sampler2D uHdr;\n"
            "uniform sampler2D uBloom;\n"
            "uniform float uExposure;\n"
            "uniform float uBloomIntensity;\n"
            "uniform int uMode;\n"
            "vec3 aces(vec3 x){ float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;\n"
            "    return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0); }\n"
            "vec3 reinhard(vec3 x){ return x/(1.0+x); }\n"
            "vec3 hejl(vec3 x){ vec3 X=max(vec3(0.0),x-0.004);\n"
            "    return (X*(6.2*X+0.5))/(X*(6.2*X+1.7)+0.06); }\n"
            "void main(){\n"
            "    vec3 hdr = texture(uHdr, vUV).rgb;\n"
            "    hdr += texture(uBloom, vUV).rgb * uBloomIntensity;\n" // add glow in linear HDR
            "    hdr *= uExposure;\n"
            "    vec3 c;\n"
            "    if (uMode == 0) { fragColor = vec4(hdr, 1.0); return; }\n"   // linear passthrough
            "    else if (uMode == 2) c = pow(reinhard(hdr), vec3(1.0/2.2));\n"
            "    else if (uMode == 3) c = hejl(hdr);\n"                       // already sRGB-encoded
            "    else c = pow(aces(hdr), vec3(1.0/2.2));\n"
            "    fragColor = vec4(c, 1.0);\n"
            "}\n";

        // Bloom bright-pass: keep only the energy above a luminance threshold.
        const char* kBrightFrag =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec2 vUV;\n"
            "out vec4 fragColor;\n"
            "uniform sampler2D uScene;\n"
            "uniform float uThreshold;\n"
            "void main(){\n"
            "    vec3 c = texture(uScene, vUV).rgb;\n"
            "    float b = max(max(c.r, c.g), c.b);\n"
            "    float keep = max(b - uThreshold, 0.0) / max(b, 1e-4);\n"
            "    fragColor = vec4(c * keep, 1.0);\n"
            "}\n";

        // Separable 9-tap Gaussian; uHorizontal picks the axis.
        const char* kBlurFrag =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec2 vUV;\n"
            "out vec4 fragColor;\n"
            "uniform sampler2D uTex;\n"
            "uniform vec2 uTexel;\n"
            "uniform float uHorizontal;\n"
            "void main(){\n"
            "    float w[5];\n"
            "    w[0]=0.227027; w[1]=0.1945946; w[2]=0.1216216; w[3]=0.054054; w[4]=0.016216;\n"
            "    vec2 dir = uHorizontal > 0.5 ? vec2(uTexel.x, 0.0) : vec2(0.0, uTexel.y);\n"
            "    vec3 result = texture(uTex, vUV).rgb * w[0];\n"
            "    for (int i = 1; i < 5; ++i){\n"
            "        result += texture(uTex, vUV + dir * float(i)).rgb * w[i];\n"
            "        result += texture(uTex, vUV - dir * float(i)).rgb * w[i];\n"
            "    }\n"
            "    fragColor = vec4(result, 1.0);\n"
            "}\n";
    }
    bool Renderer::InitShader(const char **ShaderSource, GLenum type)
    {
        if (!m_ShaderProgram)
        {
            bool suc = CreateProgram();
            if (!suc)
            {
                std::cout << "Failed to create shader program" << std::endl;
                return false;
            }
        }
        GLuint Shader = glCreateShader(type);

        glShaderSource(Shader, 1, ShaderSource, NULL);
        glCompileShader(Shader);

        GLint success = 0;
        GLchar log[1024] = {0};

        glGetShaderiv(Shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(Shader, sizeof(log), NULL, log);
            std::cout << "Error compiling shader: " << log << std::endl;
            return false;
        }

        glAttachShader(m_ShaderProgram, Shader);

        return true;
    }
    bool Renderer::InitShader(const char *ShaderSource, GLenum type)
    {
        if (!m_ShaderProgram)
        {
            bool suc = CreateProgram();
            if (!suc)
            {
                std::cout << "Failed to create shader program" << std::endl;
                return false;
            }
        }
        GLuint Shader = glCreateShader(type);

        glShaderSource(Shader, 1, &ShaderSource, NULL);
        glCompileShader(Shader);

        GLint success = 0;
        GLchar log[1024] = {0};

        glGetShaderiv(Shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(Shader, sizeof(log), NULL, log);
            std::cout << "Error compiling shader: " << log << std::endl;
            std::cout << ShaderSource << std::endl;
            return false;
        }

        glAttachShader(m_ShaderProgram, Shader);
        glDeleteShader(Shader);
        return true;
    }
    bool Renderer::InitShaderFromFile(const char *path, GLenum type)
    {

        std::string source;
        std::ifstream in_file(path, std::ios::in);

        if (!in_file.is_open())
        {
            std::cout << "Failed to open file: " << path << std::endl;
            return false;
        }
        std::string line = "";
        while (getline(in_file, line))
            source.append(line + "\n");
        in_file.close();

        source = AdaptShaderForPlatform(source); // GLES "300 es" -> desktop "330 core"
        const char *source_c_str = source.c_str();
        return InitShader(source_c_str, type);
    }
    bool Renderer::CreateProgram()
    {
        m_ShaderProgram = glCreateProgram();
        if (!m_ShaderProgram)
        {
            std::cout << "Failed to create shader program" << std::endl;
            return false;
        }
        return true;
    }
    void Renderer::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float angle, const glm::vec2& rotationCenter)
    {

        glm::vec2 adjusted =  { position.x - rotationCenter.x, position.y - rotationCenter.y};
        glm::vec2 rotated = glm::rotate(adjusted, glm::radians(angle));
        glm::vec2 final = { rotated.x + rotationCenter.x, rotated.y + rotationCenter.y};
        DrawQuad({ final.x, final.y, 0.0f}, size, color);
    }
    void Renderer::DrawQuad(const glm::vec2 &position, const glm::vec2 &size, const glm::vec4 &color)
    {
        DrawQuad({position.x, position.y, 0.0f}, size, color);
    }
    void Renderer::DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), {size.x, size.y, 1.0f});
        DrawQuad(transform, color);
    }
    void Renderer::DrawQuad(const glm::mat4 &position, const glm::vec4 &color)
    {
        // The batch is drawn with glDrawArrays (non-indexed), so a quad must emit
        // two full triangles = 6 vertices, all fields initialized (an uninitialized
        // texIndex would otherwise sample a stray texture).
        static const int order[6] = {0, 1, 2, 0, 2, 3};
        static const glm::vec2 uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        if (m_QuadIndexCount >= MaxIndices)
        {
            NextBatch();
        }
        for (int k = 0; k < 6; k++)
        {
            const int i = order[k];
            m_QuadVertexBufferPtr->position = position * m_QuadVertexPositions[i];
            m_QuadVertexBufferPtr->color = color;
            m_QuadVertexBufferPtr->normal = glm::vec3(0.0f, 0.0f, 1.0f);
            m_QuadVertexBufferPtr->texCoord = uvs[i];
            m_QuadVertexBufferPtr->texIndex = 0;
            m_QuadVertexBufferPtr++;
        }
        m_QuadIndexCount += 6;
        QuadCount++;
    }
    void Renderer::DrawCube(const glm::vec3 &position, const glm::vec3 &size, const glm::vec4 &color, float angle = 0.0f, unsigned int textureIndex)
    {
        // Standard TRS: translate * rotate(yaw) * scale, so position and size
        // stay independent (the cube is centred at 'position' and sized by 'size').
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), glm::radians(angle), {0.0f, 1.0f, 0.0f}) * glm::scale(glm::mat4(1.0f), size);
        DrawCube(transform, color, textureIndex);
    }

    void Renderer::DrawCube(const glm::mat4 &transform, const glm::vec4 &color, unsigned int textureIndex)
    {
        constexpr size_t cubeVertexCount = 36;
        if (m_QuadIndexCount >= MaxIndices)
            NextBatch();

        const glm::mat3 normalMat = glm::mat3(transform);
        for (size_t i = 0; i < cubeVertexCount; i += 3)
        {
            glm::vec3 localN = glm::cross((glm::vec3)(m_CubeVertexPositions[QuadIndices[i + 1]] - m_CubeVertexPositions[QuadIndices[i]]), (glm::vec3)(m_CubeVertexPositions[QuadIndices[i + 2]] - m_CubeVertexPositions[QuadIndices[i]]));
            glm::vec3 normal = glm::normalize(normalMat * localN);
            for (int k = 0; k < 3; ++k)
            {
                m_QuadVertexBufferPtr->position = transform * m_CubeVertexPositions[QuadIndices[i + k]];
                m_QuadVertexBufferPtr->color = color;
                m_QuadVertexBufferPtr->normal = normal;
                m_QuadVertexBufferPtr->texCoord = m_CubeVertexTexCoords[QuadIndices[i + k] % 4];
                m_QuadVertexBufferPtr->texIndex = textureIndex;
                m_QuadVertexBufferPtr++;
            }
        }
        m_QuadIndexCount += 36;
        QuadCount += 6;
    }
    void Renderer::ConfigureMeshVertexAttribs()
    {
        // Assumes the target VAO and its GL_ARRAY_BUFFER are already bound.
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)offsetof(MeshVertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)offsetof(MeshVertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)offsetof(MeshVertex, normal));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)offsetof(MeshVertex, texCoord));
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(MeshVertex), (void *)offsetof(MeshVertex, texIndex));
    }

    void Renderer::InitMeshBuffers()
    {
        glGenVertexArrays(1, &m_MeshVAO);
        glGenBuffers(1, &m_MeshVBO);
        glGenBuffers(1, &m_MeshIBO);

        glBindVertexArray(m_MeshVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_MeshVBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_MeshIBO);
        ConfigureMeshVertexAttribs();

        m_MeshBuffersReady = true;
    }
    void Renderer::DrawMesh(const MeshData &mesh)
    {
        if (mesh.indices.empty())
            return;
        if (!m_MeshBuffersReady)
            InitMeshBuffers();

        ApplyFrameUniforms(); // uses the active projection set by BeginStream
        if (u_ShadeMode >= 0)
            glUniform1i(u_ShadeMode, 1); // world meshes use procedural terrain shading

        glBindVertexArray(m_MeshVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_MeshVBO);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(MeshVertex), mesh.vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_MeshIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_DYNAMIC_DRAW);

        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);

        DrawCallCount++;
        Triangles += (int)mesh.TriangleCount();
    }
    void Renderer::DrawMesh(const MeshData &mesh, std::uint32_t cacheKey, std::uint32_t version)
    {
        if (mesh.indices.empty())
            return;

        ApplyFrameUniforms(); // uses the active projection set by BeginStream
        if (u_ShadeMode >= 0)
            glUniform1i(u_ShadeMode, 1); // world meshes use procedural terrain shading

        MeshCacheEntry &e = m_MeshCache[cacheKey];
        if (e.vao == 0)
        {
            glGenVertexArrays(1, &e.vao);
            glGenBuffers(1, &e.vbo);
            glGenBuffers(1, &e.ibo);
            glBindVertexArray(e.vao);
            glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, e.ibo);
            ConfigureMeshVertexAttribs();
            e.version = 0xFFFFFFFFu; // force the first upload
        }
        else
        {
            glBindVertexArray(e.vao);
        }

        // Only re-upload when the chunk's mesh actually changed (LOD / sculpt /
        // regen). Static terrain therefore stops re-uploading every frame.
        if (e.version != version)
        {
            glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(MeshVertex), mesh.vertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, e.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);
            e.indexCount = (GLsizei)mesh.indices.size();
            e.version = version;
        }

        glDrawElements(GL_TRIANGLES, e.indexCount, GL_UNSIGNED_INT, 0);

        DrawCallCount++;
        Triangles += (int)mesh.TriangleCount();
    }
    void Renderer::NextBatch()
    {
        Flush(); // Flush() now empties the batch, so the next quad starts clean
    }
    void Renderer::ResetBatch()
    {
        m_QuadIndexCount = 0;
        m_QuadVertexBufferPtr = m_QuadVertexBufferBase;
    }
    void Renderer::StartBatch()
    {
        // Frame start: clear the per-frame stats as well as the vertex batch.
        DrawCallCount = 0;
        Triangles = 0;
        QuadCount = 0;
        ResetBatch();
    }
    void Renderer::ApplyFrameUniforms()
    {
        glUseProgram(m_ShaderProgram);
        glUniformMatrix4fv(u_ProjectionMat, 1, GL_FALSE, &m_ActiveProjection[0][0]);
        glUniformMatrix4fv(u_ViewMat, 1, GL_FALSE, &viewMatrix[0][0]);
        if (u_ShadeMode >= 0)
            glUniform1i(u_ShadeMode, m_ShadeMode);
        if (u_CameraPos >= 0)
            glUniform3fv(u_CameraPos, 1, &m_CameraWorldPos[0]);
    }
    void Renderer::Flush()
    {
        ApplyFrameUniforms();

        if (m_QuadIndexCount == 0)
            return;
        uint32_t dataSize = (uint32_t)((uint8_t *)m_QuadVertexBufferPtr - (uint8_t *)m_QuadVertexBufferBase);

        SetVertexBufferData(m_QuadVertexBufferBase, dataSize);
        BindVertexArray();
        glDrawArrays(GL_TRIANGLES, 0, m_QuadIndexCount);
        DrawCallCount++;
        ResetBatch(); // empty the batch so a following Flush() won't redraw it
    }
    bool Renderer::Compile()
    {


        if (!m_ShaderProgram)
        {
            bool suc = CreateProgram();
            if (!suc)
            {
                std::cout << "Failed to create shader program" << std::endl;
                return false;
            }
        }
        if (!m_VertexShader)
        {
            bool suc = InitShaderFromFile("Assets/Shaders/2D/VertexShader.glsl", GL_VERTEX_SHADER); // Default vertex shader
            if (!suc)
            {
                std::cout << "Failed to load vertex shader" << std::endl;
                return false;
            }
        }
        if (!m_FragmentShader)
        {
            bool suc = InitShaderFromFile("Assets/Shaders/2D/FragmentShader.glsl", GL_FRAGMENT_SHADER); // Default fragment shader
            if (!suc)
            {
                std::cout << "Failed to load fragment shader" << std::endl;
                return false;
            }
        }

        GLint success = 0;
        GLchar log[1024] = {0};

        glLinkProgram(m_ShaderProgram);
        glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(m_ShaderProgram, sizeof(log), NULL, log);
            std::cout << "Error linking shader program: " << log << std::endl;
            return false;
        }

        glValidateProgram(m_ShaderProgram);
        glGetProgramiv(m_ShaderProgram, GL_VALIDATE_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(m_ShaderProgram, sizeof(log), NULL, log);
            std::cout << "Invalid shader program: " << log << std::endl;
            return false;
        }
        for (int i = 1; i < 17; i++) {
            std::string name = "tex" + std::to_string(i);

            glUniform1i(glGetUniformLocation(m_ShaderProgram, name.c_str()) , i - 1);
        }

         

        InitiateVertexArray();
        InitiateVertexBuffer(MaxVertices * sizeof(Vertex));
        AddVertexBuffer();
        u_ProjectionMat = glGetUniformLocation(m_ShaderProgram, "projection");
        u_ViewMat = glGetUniformLocation(m_ShaderProgram, "view");
        u_ShadeMode = glGetUniformLocation(m_ShaderProgram, "uShadeMode");
        u_CameraPos = glGetUniformLocation(m_ShaderProgram, "uCameraPos");
        u_AmbientColor = glGetUniformLocation(m_ShaderProgram, "uAmbientColor");
        u_AmbientIntensity = glGetUniformLocation(m_ShaderProgram, "uAmbientIntensity");
        u_SunDir = glGetUniformLocation(m_ShaderProgram, "uSunDir");
        u_SunColor = glGetUniformLocation(m_ShaderProgram, "uSunColor");
        u_SunIntensity = glGetUniformLocation(m_ShaderProgram, "uSunIntensity");
        u_PointCount = glGetUniformLocation(m_ShaderProgram, "uPointCount");
        u_FogColor = glGetUniformLocation(m_ShaderProgram, "uFogColor");
        u_FogDensity = glGetUniformLocation(m_ShaderProgram, "uFogDensity");

        // Cache the indexed point-light uniform locations up front.
        for (int i = 0; i < kMaxShaderPointLights; ++i)
        {
            const std::string s = std::to_string(i);
            u_PointPos[i]       = glGetUniformLocation(m_ShaderProgram, ("uPointPos[" + s + "]").c_str());
            u_PointColor[i]     = glGetUniformLocation(m_ShaderProgram, ("uPointColor[" + s + "]").c_str());
            u_PointIntensity[i] = glGetUniformLocation(m_ShaderProgram, ("uPointIntensity[" + s + "]").c_str());
            u_PointRadius[i]    = glGetUniformLocation(m_ShaderProgram, ("uPointRadius[" + s + "]").c_str());
        }

        m_QuadVertexBufferBase = new Vertex[MaxVertices];
        QuadIndices = new uint32_t[MaxIndices];
        for (uint32_t i = 0; i < MaxIndices; i += 36)
        {
            QuadIndices[i + 0] = 0; //Front
            QuadIndices[i + 1] = 1;
            QuadIndices[i + 2] = 2;

            QuadIndices[i + 3] = 0; //Front
            QuadIndices[i + 4] = 2;
            QuadIndices[i + 5] = 3;

            QuadIndices[i + 6] = 6; // Back
            QuadIndices[i + 7] = 5;
            QuadIndices[i + 8] = 4;

            QuadIndices[i + 9] = 4; //Back
            QuadIndices[i + 10] = 7;
            QuadIndices[i + 11] = 6;

            QuadIndices[i + 12] = 1; // Right
            QuadIndices[i + 13] = 5;
            QuadIndices[i + 14] = 6;

            QuadIndices[i + 15] = 1; //Right
            QuadIndices[i + 16] = 6;
            QuadIndices[i + 17] = 2;

            QuadIndices[i + 18] = 4; // Left
            QuadIndices[i + 19] = 0;
            QuadIndices[i + 20] = 3;
            
            QuadIndices[i + 21] = 4; //Left
            QuadIndices[i + 22] = 3;
            QuadIndices[i + 23] = 7;
            
            QuadIndices[i + 24] = 4; // Bottom
            QuadIndices[i + 25] = 5;
            QuadIndices[i + 26] = 0;
            
            QuadIndices[i + 27] = 5; // Bottom
            QuadIndices[i + 28] = 1;
            QuadIndices[i + 29] = 0;

            QuadIndices[i + 30] = 3; // Top
            QuadIndices[i + 31] = 2;
            QuadIndices[i + 32] = 6;

            QuadIndices[i + 33] = 3; // Top
            QuadIndices[i + 34] = 6;
            QuadIndices[i + 35] = 7;

        }
        m_QuadVertexPositions[0] = {-0.5f, -0.5f, 0.0f, 1.0f};
        m_QuadVertexPositions[1] = {0.5f, -0.5f, 0.0f, 1.0f};
        m_QuadVertexPositions[2] = {0.5f, 0.5f, 0.0f, 1.0f};
        m_QuadVertexPositions[3] = {-0.5f, 0.5f, 0.0f, 1.0f};

        m_CubeVertexPositions[0] = {-0.5f, -0.5f, 0.5f, 1.0f};
        m_CubeVertexPositions[1] = {0.5f, -0.5f, 0.5f, 1.0f};
        m_CubeVertexPositions[2] = {0.5f, 0.5f, 0.5f, 1.0f};
        m_CubeVertexPositions[3] = {-0.5f, 0.5f, 0.5f, 1.0f};
        m_CubeVertexPositions[4] = {-0.5f, -0.5f, -0.5f, 1.0f};
        m_CubeVertexPositions[5] = {0.5f, -0.5f, -0.5f, 1.0f};
        m_CubeVertexPositions[6] = {0.5f, 0.5f, -0.5f, 1.0f};
        m_CubeVertexPositions[7] = {-0.5f, 0.5f, -0.5f, 1.0f};

        m_CubeVertexTexCoords[0] = {0.0f, 0.0f};
        m_CubeVertexTexCoords[1] = {1.0f, 0.0f};
        m_CubeVertexTexCoords[2] = {1.0f, 1.0f};
        m_CubeVertexTexCoords[3] = {0.0f, 1.0f};
        //TODO: Change texture coordinates to allow uv mapping


        return true;
    }
    void Renderer::AddVertexBuffer()
    {
        BindVertexArray();
        
        glEnableVertexAttribArray(0);
        BindVertexBuffer();
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); //Position
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12); //Color
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)28); //Normal
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)40); //TexCoords
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(Vertex), (void*)48); //TexIndex
    }
    void Renderer::SetVertexBufferData(const void *data, GLuint size)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    void Renderer::BeginStream(Camera& camera)
    {
        viewMatrix = camera.GetViewMatrix();
        cameraZoom = camera.getZoom();
        m_CameraWorldPos = camera.m_Position;
        m_ShadeMode = 0;
        // Far plane follows the fidelity dial so distant terrain isn't clipped.
        m_ActiveProjection = glm::perspective(glm::radians(cameraZoom), 16.0f / 9.0f, 0.1f,
                                              QualitySettings::Get().renderDistance);
        StartBatch();
    }
    void Renderer::EndStream()
    {
        Flush();
    }
    void Renderer::Begin2D(float width, float height)
    {
        Flush(); // make sure any pending 3D geometry is drawn first
        // Top-left origin orthographic projection in pixels; identity view.
        m_ActiveProjection = glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
        viewMatrix = glm::mat4(1.0f);
        m_ShadeMode = 2; // unlit
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Flush() above already emptied the batch; HUD quads start clean.
    }
    void Renderer::End2D()
    {
        Flush();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        m_ShadeMode = 0;
    }
    void Renderer::SetLights(const LightManager& lights, const glm::vec3& cameraPos)
    {
        glUseProgram(m_ShaderProgram);

        glUniform3fv(u_AmbientColor, 1, &lights.ambient.color[0]);
        glUniform1f(u_AmbientIntensity, lights.ambient.intensity);

        glm::vec3 sunDir = glm::normalize(lights.sun.direction);
        glUniform3fv(u_SunDir, 1, &sunDir[0]);
        glUniform3fv(u_SunColor, 1, &lights.sun.color[0]);
        glUniform1f(u_SunIntensity, lights.sun.intensity);

        glUniform3fv(u_FogColor, 1, &lights.fogColor[0]);
        glUniform1f(u_FogDensity, lights.fogDensity);

        const int budget = std::min(kMaxShaderPointLights, QualitySettings::Get().maxLights);
        std::vector<PointLight> pts = lights.SelectPoints(cameraPos, budget);
        glUniform1i(u_PointCount, (int)pts.size());
        for (size_t i = 0; i < pts.size() && i < (size_t)kMaxShaderPointLights; ++i)
        {
            glUniform3fv(u_PointPos[i], 1, &pts[i].position[0]);
            glUniform3fv(u_PointColor[i], 1, &pts[i].color[0]);
            glUniform1f(u_PointIntensity[i], pts[i].intensity);
            glUniform1f(u_PointRadius[i], pts[i].radius);
        }
    }
    void Renderer::BindVertexBuffer()
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
    }
    void Renderer::InitiateVertexBuffer(float *vertices, GLuint size)
    {
        glGenBuffers(1, &m_VertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
    }
    void Renderer::InitiateVertexBuffer(GLuint size)
    {
        glGenBuffers(1, &m_VertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    }
    void Renderer::InitiateVertexArray()
    {
        glGenVertexArrays(1, &m_VertexArray);
    }
    void Renderer::BindVertexArray()
    {
        glBindVertexArray(m_VertexArray);
    }
    void Renderer::UnbindVertexArray()
    {
        glBindVertexArray(0);
    }
    void Renderer::UnbindVertexBuffer()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    void Renderer::GenerateFrameBuffer()
    {
        const int W = 2560, H = 1440;
        glViewport(0, 0, W, H);

#if defined(KE_PLATFORM_WEB)
        // WebGL2 needs this extension enabled before a float texture is renderable.
        emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(), "EXT_color_buffer_float");
#endif

        glGenFramebuffers(1, &m_FrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);

        glGenTextures(1, &m_Texture);
        glBindTexture(GL_TEXTURE_2D, m_Texture);
        // HDR (half-float) colour target so lighting can exceed 1.0 before tonemap.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, W, H, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
        // Sampled 1:1 by the resolve pass, so nearest avoids any float-filter dependency.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

        glGenRenderbuffers(1, &m_RenderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RenderBuffer);

        m_HdrEnabled = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        if (!m_HdrEnabled)
        {
            // Float target not renderable here: fall back to 8-bit (tonemap still runs).
            glBindTexture(GL_TEXTURE_2D, m_Texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "Framebuffer not complete" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        BuildTonemapResources(W, H);
    }

    // Build the LDR resolve target + the fullscreen tonemap program.
    void Renderer::BuildTonemapResources(int width, int height)
    {
        glGenFramebuffers(1, &m_ResolveFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_ResolveFBO);

        glGenTextures(1, &m_ResolveTexture);
        glBindTexture(GL_TEXTURE_2D, m_ResolveTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // ImGui scales this one
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ResolveTexture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Resolve framebuffer not complete" << std::endl;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_TonemapProgram = BuildStandaloneProgram(kTonemapVert, kTonemapFrag);
        if (m_TonemapProgram)
        {
            u_TmHdr            = glGetUniformLocation(m_TonemapProgram, "uHdr");
            u_TmBloom          = glGetUniformLocation(m_TonemapProgram, "uBloom");
            u_TmExposure       = glGetUniformLocation(m_TonemapProgram, "uExposure");
            u_TmBloomIntensity = glGetUniformLocation(m_TonemapProgram, "uBloomIntensity");
            u_TmMode           = glGetUniformLocation(m_TonemapProgram, "uMode");
        }
        glGenVertexArrays(1, &m_TonemapVAO); // empty VAO; positions come from gl_VertexID

        BuildBloomResources(width / 2, height / 2); // bloom runs at half resolution
    }

    void Renderer::BuildBloomResources(int width, int height)
    {
        m_BloomW = width;
        m_BloomH = height;

        auto makeTarget = [&](GLuint& fbo, GLuint& tex) {
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            // RGBA16F is texture-filterable in ES3/WebGL2, so the blur can sample linearly.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "Bloom framebuffer not complete" << std::endl;
        };
        makeTarget(m_BrightFBO, m_BrightTex);
        makeTarget(m_BlurFBO[0], m_BlurTex[0]);
        makeTarget(m_BlurFBO[1], m_BlurTex[1]);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_BrightProgram = BuildStandaloneProgram(kTonemapVert, kBrightFrag);
        if (m_BrightProgram)
        {
            u_BrScene     = glGetUniformLocation(m_BrightProgram, "uScene");
            u_BrThreshold = glGetUniformLocation(m_BrightProgram, "uThreshold");
        }
        m_BlurProgram = BuildStandaloneProgram(kTonemapVert, kBlurFrag);
        if (m_BlurProgram)
        {
            u_BlTex        = glGetUniformLocation(m_BlurProgram, "uTex");
            u_BlTexel      = glGetUniformLocation(m_BlurProgram, "uTexel");
            u_BlHorizontal = glGetUniformLocation(m_BlurProgram, "uHorizontal");
        }
    }

    // Bright-pass the HDR scene then ping-pong a separable blur. Returns the
    // texture holding the final bloom (or 0 if bloom can't run this frame).
    GLuint Renderer::RenderBloom()
    {
        if (!bloomEnabled || !m_BrightProgram || !m_BlurProgram || !m_TonemapVAO)
            return 0;

        glViewport(0, 0, m_BloomW, m_BloomH);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glBindVertexArray(m_TonemapVAO);

        // 1) Bright-pass scene -> m_BrightTex.
        glBindFramebuffer(GL_FRAMEBUFFER, m_BrightFBO);
        glUseProgram(m_BrightProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_Texture);
        if (u_BrScene >= 0)     glUniform1i(u_BrScene, 0);
        if (u_BrThreshold >= 0) glUniform1f(u_BrThreshold, bloomThreshold);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 2) Separable blur, ping-ponging between the two blur targets.
        glUseProgram(m_BlurProgram);
        if (u_BlTex >= 0)   glUniform1i(u_BlTex, 0);
        if (u_BlTexel >= 0) glUniform2f(u_BlTexel, 1.0f / (float)m_BloomW, 1.0f / (float)m_BloomH);

        GLuint src = m_BrightTex;
        const int kIterations = 2; // 2 H+V passes => a soft, wide glow
        for (int i = 0; i < kIterations; ++i)
        {
            // Horizontal: src -> blur[0]
            glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO[0]);
            if (u_BlHorizontal >= 0) glUniform1f(u_BlHorizontal, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, src);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            // Vertical: blur[0] -> blur[1]
            glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO[1]);
            if (u_BlHorizontal >= 0) glUniform1f(u_BlHorizontal, 0.0f);
            glBindTexture(GL_TEXTURE_2D, m_BlurTex[0]);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            src = m_BlurTex[1];
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        return m_BlurTex[1];
    }

    void Renderer::ResolveToneMap()
    {
        if (!m_TonemapProgram || !m_ResolveFBO)
            return; // fall back to displaying the raw scene texture

        // Bloom first (writes its own half-res targets), then the final resolve.
        const GLuint bloomTex = RenderBloom();
        const float bloomAmount = (bloomTex != 0) ? bloomIntensity : 0.0f;

        glBindFramebuffer(GL_FRAMEBUFFER, m_ResolveFBO);
        glViewport(0, 0, 2560, 1440);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glUseProgram(m_TonemapProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_Texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomTex != 0 ? bloomTex : m_Texture); // valid sampler even when off
        if (u_TmHdr >= 0)            glUniform1i(u_TmHdr, 0);
        if (u_TmBloom >= 0)          glUniform1i(u_TmBloom, 1);
        if (u_TmExposure >= 0)       glUniform1f(u_TmExposure, tonemapExposure);
        if (u_TmBloomIntensity >= 0) glUniform1f(u_TmBloomIntensity, bloomAmount);
        if (u_TmMode >= 0)           glUniform1i(u_TmMode, tonemapMode);

        glBindVertexArray(m_TonemapVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
    }
    void Renderer::BindFrameBuffer()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
    }
    void Renderer::UnbindFrameBuffer()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void Renderer::RescaleFrameBuffer(int width, int height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }
        
        //TODO: Get virutal aspect ratio from user
        int virtualWidth = 2560;
        int virtualHeight = 1440;

        float targetAspectRatio = (float)virtualWidth / (float)virtualHeight;
        int newWidth = width;
        int newHeight = (int)(float(newWidth) / targetAspectRatio + 0.5f);
        if (newHeight > height)
        {
            newHeight = height;
            newWidth = (int)(float(newHeight) * targetAspectRatio + 0.5f);
        }

        float fbWidth = float(newWidth) / float(width);
        float fbHeight = float(newHeight) / float(height);
        BindFrameBuffer();
        glBindTexture(GL_TEXTURE_2D, m_Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, virtualWidth, virtualHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, virtualWidth, virtualHeight);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RenderBuffer);
        UnbindFrameBuffer();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Renderer::LoadTexture(const char* path) {
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);  
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glActiveTexture(GLTextureMapIndex(m_CurrentTextureIndex));
            glBindTexture(GL_TEXTURE_2D, texture);
        }
        else

        {
            std::cout << "Failed to load texture" << std::endl;
        }
        stbi_image_free(data);
        m_CurrentTextureIndex++;
        
    }
    float closestResolution(int n, int multiple) {
        
        int rem = n % multiple;
        n = n - rem;
        return (float)n;
    }
}

