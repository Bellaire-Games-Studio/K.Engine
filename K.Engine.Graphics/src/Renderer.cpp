#include <Renderer.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <Image/stb_image.h>
#define GLTextureMapIndex(x) (x == 0 ? GL_TEXTURE0 : x == 1 ? GL_TEXTURE1 : x == 2 ? GL_TEXTURE2 : x == 3 ? GL_TEXTURE3 : x == 4 ? GL_TEXTURE4 : x == 5 ? GL_TEXTURE5 : x == 6 ? GL_TEXTURE6 : x == 7 ? GL_TEXTURE7 : x == 8 ? GL_TEXTURE8 : x == 9 ? GL_TEXTURE9 : x == 10 ? GL_TEXTURE10 : x == 11 ? GL_TEXTURE11 : x == 12 ? GL_TEXTURE12 : x == 13 ? GL_TEXTURE13 : x == 14 ? GL_TEXTURE14 : x == 15 ? GL_TEXTURE15 : x == 16 ? GL_TEXTURE16 : x == 17 ? GL_TEXTURE17 : x == 18 ? GL_TEXTURE18 : x == 19 ? GL_TEXTURE19 : x == 20 ? GL_TEXTURE20 : x == 21 ? GL_TEXTURE21 : x == 22 ? GL_TEXTURE22 : x == 23 ? GL_TEXTURE23 : x == 24 ? GL_TEXTURE24 : x == 25 ? GL_TEXTURE25 : x == 26 ? GL_TEXTURE26 : x == 27 ? GL_TEXTURE27 : x == 28 ? GL_TEXTURE28 : x == 29 ? GL_TEXTURE29 : x == 30 ? GL_TEXTURE30 : x == 31 ? GL_TEXTURE31 : 0)

namespace KDot
{

    Renderer::~Renderer()
    {
        glDeleteProgram(m_ShaderProgram);
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
        constexpr size_t quadVertexCount = 4;
        if (m_QuadIndexCount >= MaxIndices)
        {
            NextBatch();
        }
        for (size_t i = 0; i < quadVertexCount; i++)
        {
            m_QuadVertexBufferPtr->position = position * m_QuadVertexPositions[i];
            m_QuadVertexBufferPtr->color = color;
            m_QuadVertexBufferPtr++;
        }
        m_QuadIndexCount += 6;
        QuadCount++;
    }
    void Renderer::DrawCube(const glm::vec3 &position, const glm::vec3 &size, const glm::vec4 &color, float angle = 0.0f, unsigned int textureIndex)
    {
        glm::mat4 transform =  glm::scale(glm::mat4(1.0f), size) * glm::rotate(glm::mat4(1.0f), glm::radians(angle), {1.0f, 1.0f, 0.0f}) * glm::translate(glm::mat4(1.0f), position);
        constexpr size_t cubeVertexCount = 36;
        if (m_QuadIndexCount >= MaxIndices)
        {
            NextBatch(); // We've reached the maximum number of QuadIndices, flush the buffer and start a new one
        }
        for (size_t i = 0; i < cubeVertexCount; i +=3)
        {
            glm::vec3 normal = glm::normalize(glm::cross((glm::vec3)(m_CubeVertexPositions[QuadIndices[i + 1]] - m_CubeVertexPositions[QuadIndices[i]]), (glm::vec3)(m_CubeVertexPositions[QuadIndices[i + 2]] - m_CubeVertexPositions[QuadIndices[i]])));
            m_QuadVertexBufferPtr->position = transform * m_CubeVertexPositions[QuadIndices[i]];
            m_QuadVertexBufferPtr->color = color;
            m_QuadVertexBufferPtr->normal = normal;
            m_QuadVertexBufferPtr->texCoord = m_CubeVertexTexCoords[QuadIndices[i] % 4];
            m_QuadVertexBufferPtr->texIndex = textureIndex;
            m_QuadVertexBufferPtr++;
            m_QuadVertexBufferPtr->position = transform * m_CubeVertexPositions[QuadIndices[i + 1]];
            m_QuadVertexBufferPtr->color = color;
            m_QuadVertexBufferPtr->normal = normal;
            m_QuadVertexBufferPtr->texCoord = m_CubeVertexTexCoords[QuadIndices[i + 1] % 4];
            m_QuadVertexBufferPtr->texIndex = textureIndex;
            m_QuadVertexBufferPtr++;
            m_QuadVertexBufferPtr->position = transform * m_CubeVertexPositions[QuadIndices[i + 2]];
            m_QuadVertexBufferPtr->color = color;
            m_QuadVertexBufferPtr->normal = normal;
            m_QuadVertexBufferPtr->texCoord = m_CubeVertexTexCoords[QuadIndices[i + 2] % 4];
            m_QuadVertexBufferPtr->texIndex = textureIndex;

            m_QuadVertexBufferPtr++;

        }
        m_QuadIndexCount += 36;
        QuadCount+= 6;
    }
    void Renderer::NextBatch()
    {
        Flush();
        StartBatch();

    }
    void Renderer::StartBatch()
    {
        m_QuadIndexCount = 0;
        DrawCallCount = 0;
        Triangles = 0;
        QuadCount = 0;
        m_QuadVertexBufferPtr = m_QuadVertexBufferBase;
    }
    void Renderer::Flush()
    {
        glm::mat4 projection = glm::perspective(glm::radians(cameraZoom), (float)16 / (float)9, 0.1f, 100.0f);
        glUniformMatrix4fv(u_ProjectionMat, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(u_ViewMat, 1, GL_FALSE, &viewMatrix[0][0]);

        if (m_QuadIndexCount == 0)
            return;
        uint32_t dataSize = (uint32_t)((uint8_t *)m_QuadVertexBufferPtr - (uint8_t *)m_QuadVertexBufferBase);
        glUseProgram(m_ShaderProgram);
        
        SetVertexBufferData(m_QuadVertexBufferBase, dataSize);
        BindVertexArray();
        glDrawArrays(GL_TRIANGLES, 0, m_QuadIndexCount);
        DrawCallCount++;
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

    void Renderer::AddIndexBuffer()
    {
        BindVertexArray();
        BindIndexBuffer();
    }
    void Renderer::BeginStream(Camera& camera)
    {
        viewMatrix = camera.GetViewMatrix();
        cameraZoom = camera.getZoom();
        StartBatch();
    }
    void Renderer::EndStream()
    {
        Flush();
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
        glViewport(0, 0, 2560, 1440);

        glGenFramebuffers(1, &m_FrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);

        glGenTextures(1, &m_Texture);
        glBindTexture(GL_TEXTURE_2D, m_Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2560, 1440, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

        glGenRenderbuffers(1, &m_RenderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 2560, 1440);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RenderBuffer);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "Framebuffer not complete" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
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

