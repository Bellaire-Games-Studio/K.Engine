#include <RHI/GLDevice.hpp>
#include <Core/ShaderUtil.hpp>
#if defined(KE_BACKEND_WEBGPU)
#include <RHI/WebGPUDevice.hpp>
#endif
#include <iostream>
#include <fstream>
#include <iterator>

namespace KDot
{
    namespace rhi
    {
        namespace
        {
            GLenum TargetFor(BufferType t)
            {
                switch (t)
                {
                    case BufferType::Vertex:  return GL_ARRAY_BUFFER;
                    case BufferType::Index:   return GL_ELEMENT_ARRAY_BUFFER;
                    case BufferType::Uniform: return GL_UNIFORM_BUFFER;
                }
                return GL_ARRAY_BUFFER;
            }

            GLenum GLTypeFor(AttribFormat f)
            {
                return AttribIsInteger(f) ? GL_UNSIGNED_INT : GL_FLOAT;
            }

            GLuint CompileShaderFile(const std::string& path, GLenum stage)
            {
                std::ifstream in(path, std::ios::in);
                if (!in.is_open())
                {
                    std::cout << "RHI: failed to open shader " << path << std::endl;
                    return 0;
                }
                std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                src = AdaptShaderForPlatform(src);

                const char* c = src.c_str();
                GLuint shader = glCreateShader(stage);
                glShaderSource(shader, 1, &c, NULL);
                glCompileShader(shader);

                GLint ok = 0;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
                if (!ok)
                {
                    char log[1024] = {0};
                    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
                    std::cout << "RHI shader compile error (" << path << "): " << log << std::endl;
                    glDeleteShader(shader);
                    return 0;
                }
                return shader;
            }
        }

        // ---- GLBuffer ----------------------------------------------------------
        GLBuffer::GLBuffer(GLenum target, std::size_t bytes, const void* initial, bool dynamic)
            : m_Target(target), m_Usage(dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW), m_Capacity(bytes)
        {
            glGenBuffers(1, &m_Id);
            glBindBuffer(m_Target, m_Id);
            glBufferData(m_Target, bytes, initial, m_Usage);
        }
        GLBuffer::~GLBuffer()
        {
            if (m_Id)
                glDeleteBuffers(1, &m_Id);
        }
        void GLBuffer::Update(const void* data, std::size_t bytes)
        {
            glBindBuffer(m_Target, m_Id);
            if (bytes > m_Capacity)
            {
                glBufferData(m_Target, bytes, data, m_Usage); // grow (reallocate)
                m_Capacity = bytes;
            }
            else
            {
                glBufferSubData(m_Target, 0, bytes, data);
            }
        }

        // ---- GLPipeline --------------------------------------------------------
        GLPipeline::GLPipeline(const PipelineDesc& desc) : m_Desc(desc)
        {
            GLuint vs = CompileShaderFile(desc.vertexShaderPath, GL_VERTEX_SHADER);
            GLuint fs = CompileShaderFile(desc.fragmentShaderPath, GL_FRAGMENT_SHADER);
            if (!vs || !fs)
            {
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
                return;
            }

            m_Program = glCreateProgram();
            glAttachShader(m_Program, vs);
            glAttachShader(m_Program, fs);
            glLinkProgram(m_Program);

            GLint linked = 0;
            glGetProgramiv(m_Program, GL_LINK_STATUS, &linked);
            glDeleteShader(vs);
            glDeleteShader(fs);
            if (!linked)
            {
                char log[1024] = {0};
                glGetProgramInfoLog(m_Program, sizeof(log), NULL, log);
                std::cout << "RHI program link error: " << log << std::endl;
                glDeleteProgram(m_Program);
                m_Program = 0;
                return;
            }

            glGenVertexArrays(1, &m_Vao); // attributes are bound per draw via BindVertexBuffer

            // Bind the per-draw constants block to uniform slot 0.
            if (!desc.constantsBlock.empty())
            {
                GLuint idx = glGetUniformBlockIndex(m_Program, desc.constantsBlock.c_str());
                if (idx != GL_INVALID_INDEX)
                    glUniformBlockBinding(m_Program, idx, 0);
            }
        }
        GLPipeline::~GLPipeline()
        {
            if (m_Program) glDeleteProgram(m_Program);
            if (m_Vao) glDeleteVertexArrays(1, &m_Vao);
        }

        // ---- GLDevice ----------------------------------------------------------
        std::unique_ptr<Buffer> GLDevice::CreateBuffer(BufferType type, std::size_t bytes,
                                                       const void* initial, bool dynamic)
        {
            return std::make_unique<GLBuffer>(TargetFor(type), bytes, initial, dynamic);
        }

        std::unique_ptr<Pipeline> GLDevice::CreatePipeline(const PipelineDesc& desc)
        {
            return std::make_unique<GLPipeline>(desc);
        }

        void GLDevice::BindPipeline(Pipeline& pipeline)
        {
            GLPipeline& p = static_cast<GLPipeline&>(pipeline);
            m_Current = &p;

            glUseProgram(p.Program());
            glBindVertexArray(p.Vao());

            const PipelineDesc& d = p.Desc();
            if (d.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            glDepthMask(d.depthWrite ? GL_TRUE : GL_FALSE);

            if (d.blend)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glDisable(GL_BLEND);
            }

            switch (d.cull)
            {
                case CullMode::None:  glDisable(GL_CULL_FACE); break;
                case CullMode::Back:  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
                case CullMode::Front: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
            }
        }

        void GLDevice::BindVertexBuffer(uint32_t binding, Buffer& buffer)
        {
            if (!m_Current)
                return;
            GLBuffer& b = static_cast<GLBuffer&>(buffer);
            const PipelineDesc& d = m_Current->Desc();

            // Find the binding's stride / instancing.
            uint32_t stride = 0;
            bool perInstance = false;
            for (const VertexBinding& vb : d.layout.bindings)
                if (vb.binding == binding) { stride = vb.stride; perInstance = vb.perInstance; }

            glBindVertexArray(m_Current->Vao());
            glBindBuffer(GL_ARRAY_BUFFER, b.Id());

            for (const VertexAttribute& a : d.layout.attributes)
            {
                if (a.binding != binding)
                    continue;
                glEnableVertexAttribArray(a.location);
                if (AttribIsInteger(a.format))
                    glVertexAttribIPointer(a.location, AttribComponentCount(a.format), GLTypeFor(a.format),
                                           stride, (void*)(std::size_t)a.offset);
                else
                    glVertexAttribPointer(a.location, AttribComponentCount(a.format), GLTypeFor(a.format),
                                          GL_FALSE, stride, (void*)(std::size_t)a.offset);
                glVertexAttribDivisor(a.location, perInstance ? 1 : 0);
            }
        }

        void GLDevice::BindUniformBuffer(uint32_t slot, Buffer& buffer)
        {
            GLBuffer& b = static_cast<GLBuffer&>(buffer);
            glBindBufferBase(GL_UNIFORM_BUFFER, slot, b.Id());
        }

        void GLDevice::DrawInstanced(uint32_t vertexCount, uint32_t instanceCount)
        {
            if (instanceCount == 0)
                return;
            glDrawArraysInstanced(GL_TRIANGLES, 0, (GLsizei)vertexCount, (GLsizei)instanceCount);
        }

        // ---- Backend factory ---------------------------------------------------
        std::unique_ptr<Device> CreateDevice()
        {
            // Backend is chosen at build time via the KE_BACKEND_* CMake options
            // (see RHI/GraphicsAPI.hpp). WebGPU is an experimental scaffold; the
            // OpenGL backend remains the default and only runnable path today.
#if defined(KE_BACKEND_WEBGPU)
            return CreateWebGPUDevice();
#else
            return std::make_unique<GLDevice>();
#endif
        }
    }
}
