#pragma once
// -----------------------------------------------------------------------------
// Experimental WebGPU backend for the RHI (KE_BACKEND_WEBGPU).
//
//  This is a SCAFFOLD: it implements rhi::Device against Emscripten's webgpu.h
//  (Dawn-style C API) and is the foundation for moving the web target off
//  WebGL2. It is NOT a runnable backend yet - the whole renderer (Renderer.cpp,
//  the editor's ImGui GL backend, all GLSL shaders) is still OpenGL, and a single
//  canvas can only hold one GPU API. See docs/WEBGPU_PORT.md for the staged plan.
//
//  Everything here compiles only when KE_BACKEND_WEBGPU is defined, so the
//  default OpenGL build is completely unaffected.
// -----------------------------------------------------------------------------
#if defined(KE_BACKEND_WEBGPU)

#include <RHI/Device.hpp>
#include <webgpu/webgpu.h>
#include <cstdint>
#include <string>

namespace KDot
{
    namespace rhi
    {
        class WGPUBufferImpl : public Buffer
        {
        public:
            WGPUBufferImpl(WGPUDevice device, WGPUQueue queue, WGPUBufferUsageFlags usage,
                           std::size_t bytes, const void* initial);
            ~WGPUBufferImpl() override;
            void Update(const void* data, std::size_t bytes) override;

            WGPUBuffer Handle() const { return m_Buffer; }
            std::size_t Size() const { return m_Size; }

        private:
            WGPUDevice  m_Device = nullptr;
            WGPUQueue   m_Queue = nullptr;
            WGPUBuffer  m_Buffer = nullptr;
            std::size_t m_Size = 0;
        };

        class WGPUPipelineImpl : public Pipeline
        {
        public:
            WGPUPipelineImpl(WGPUDevice device, const PipelineDesc& desc, WGPUTextureFormat colorFormat);
            ~WGPUPipelineImpl() override;
            bool Valid() const override { return m_Pipeline != nullptr; }

            WGPURenderPipeline   Handle() const { return m_Pipeline; }
            WGPUBindGroupLayout  BindGroupLayout() const { return m_BindGroupLayout; }

        private:
            WGPURenderPipeline  m_Pipeline = nullptr;
            WGPUBindGroupLayout m_BindGroupLayout = nullptr;
        };

        // Implements the (immediate-style) rhi::Device by recording into a render
        // pass. The render pass is owned by the device between BeginFrame/EndFrame
        // (the hooks the full port will call from the main loop).
        class WebGPUDevice : public Device
        {
        public:
            WebGPUDevice();
            ~WebGPUDevice() override;

            std::unique_ptr<Buffer>   CreateBuffer(BufferType type, std::size_t bytes,
                                                   const void* initial, bool dynamic) override;
            std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc) override;

            void BindPipeline(Pipeline& pipeline) override;
            void BindVertexBuffer(uint32_t binding, Buffer& buffer) override;
            void BindUniformBuffer(uint32_t slot, Buffer& buffer) override;
            void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount) override;

            const char* BackendName() const override { return "WebGPU"; }

            bool Ready() const { return m_Device != nullptr; }

            // Frame hooks (not part of the immediate rhi::Device interface yet -
            // the full port wires these into the main loop; see WEBGPU_PORT.md).
            void BeginFrame(WGPUTextureView target);
            void EndFrame();

            WGPUTextureFormat ColorFormat() const { return m_ColorFormat; }

        private:
            WGPUDevice              m_Device = nullptr;
            WGPUQueue               m_Queue = nullptr;
            WGPUTextureFormat       m_ColorFormat = WGPUTextureFormat_BGRA8Unorm;
            WGPUCommandEncoder      m_Encoder = nullptr;
            WGPURenderPassEncoder   m_Pass = nullptr;
            WGPUPipelineImpl*       m_CurrentPipeline = nullptr;
        };

        std::unique_ptr<Device> CreateWebGPUDevice();
    }
}

#endif // KE_BACKEND_WEBGPU
