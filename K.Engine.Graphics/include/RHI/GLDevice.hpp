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

        class GLTexture : public Texture
        {
        public:
            explicit GLTexture(const TextureDesc& desc);
            ~GLTexture() override;
            uint64_t NativeHandle() const override { return m_Id; }
            uint32_t Width() const override { return m_Width; }
            uint32_t Height() const override { return m_Height; }

            GLuint Id() const { return m_Id; }

        private:
            GLuint   m_Id = 0;
            uint32_t m_Width = 0;
            uint32_t m_Height = 0;
            bool     m_Owned = true; // false when wrapping an external (borrowed) id
        };

        class GLRenderTarget : public RenderTarget
        {
        public:
            explicit GLRenderTarget(const RenderTargetDesc& desc);
            ~GLRenderTarget() override;
            uint32_t Width() const override { return m_Width; }
            uint32_t Height() const override { return m_Height; }
            bool Complete() const override { return m_Complete; }

            GLuint Fbo() const { return m_Fbo; }

        private:
            GLuint   m_Fbo = 0;
            uint32_t m_Width = 0;
            uint32_t m_Height = 0;
            bool     m_Complete = false;
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

            std::unique_ptr<Texture>      CreateTexture(const TextureDesc& desc) override;
            std::unique_ptr<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) override;
            void BeginRenderPass(RenderTarget* target, const RenderPassDesc& desc) override;
            void EndRenderPass() override;
            void BindTexture(uint32_t slot, Texture& texture) override;
            void Draw(uint32_t vertexCount) override;

            const char* BackendName() const override { return "OpenGL"; }

        private:
            GLPipeline* m_Current = nullptr;
        };
    }
}
