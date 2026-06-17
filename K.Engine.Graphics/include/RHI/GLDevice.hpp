#pragma once
#include <RHI/Device.hpp>
#include <Platform/GL.hpp>

namespace KDot
{
    namespace rhi
    {
        class GLBuffer : public Buffer
        {
        public:
            GLBuffer(GLenum target, std::size_t bytes, const void* initial, bool dynamic);
            ~GLBuffer() override;
            void Update(const void* data, std::size_t bytes) override;

            GLuint Id() const { return m_Id; }
            GLenum Target() const { return m_Target; }

        private:
            GLuint      m_Id = 0;
            GLenum      m_Target;
            GLenum      m_Usage;
            std::size_t m_Capacity = 0;
        };

        class GLPipeline : public Pipeline
        {
        public:
            explicit GLPipeline(const PipelineDesc& desc);
            ~GLPipeline() override;
            bool Valid() const override { return m_Program != 0; }

            GLuint              Program() const { return m_Program; }
            GLuint              Vao() const { return m_Vao; }
            const PipelineDesc& Desc() const { return m_Desc; }

        private:
            PipelineDesc m_Desc;
            GLuint       m_Program = 0;
            GLuint       m_Vao = 0;
        };

        class GLDevice : public Device
        {
        public:
            std::unique_ptr<Buffer>   CreateBuffer(BufferType type, std::size_t bytes,
                                                   const void* initial, bool dynamic) override;
            std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc) override;

            void BindPipeline(Pipeline& pipeline) override;
            void BindVertexBuffer(uint32_t binding, Buffer& buffer) override;
            void BindUniformBuffer(uint32_t slot, Buffer& buffer) override;
            void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount) override;

            const char* BackendName() const override { return "OpenGL"; }

        private:
            GLPipeline* m_Current = nullptr;
        };
    }
}
