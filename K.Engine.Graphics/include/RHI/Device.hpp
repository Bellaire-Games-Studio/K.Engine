#pragma once
#include <RHI/Types.hpp>
#include <memory>
#include <cstddef>

namespace KDot
{
    namespace rhi
    {
        // GPU buffer (vertex / index / uniform). Update() reuploads its contents.
        class Buffer
        {
        public:
            virtual ~Buffer() = default;
            virtual void Update(const void* data, std::size_t bytes) = 0;
        };

        // A compiled shader program + vertex layout + fixed-function state.
        class Pipeline
        {
        public:
            virtual ~Pipeline() = default;
            virtual bool Valid() const = 0;
        };

        // A 2D texture (sampled input and/or render-target attachment).
        class Texture
        {
        public:
            virtual ~Texture() = default;
            // Backend-native handle (GL texture id today). Needed for interop such
            // as handing the resolved scene texture to ImGui::Image.
            virtual uint64_t NativeHandle() const = 0;
            virtual uint32_t Width() const = 0;
            virtual uint32_t Height() const = 0;
        };

        // Where a render pass draws: one or more colour attachments + optional depth.
        struct RenderTargetDesc
        {
            std::vector<Texture*> colors;
            Texture*              depth = nullptr;
        };

        // An off-screen framebuffer (GL FBO today).
        class RenderTarget
        {
        public:
            virtual ~RenderTarget() = default;
            virtual uint32_t Width() const = 0;
            virtual uint32_t Height() const = 0;
        };

        struct RenderPassDesc
        {
            bool  clear = false;
            float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        };

        // The rendering device. An "immediate" interface (bind + draw) that maps
        // directly onto OpenGL and is recorded into a command buffer by the
        // explicit backends (Vulkan/WebGPU).
        class Device
        {
        public:
            virtual ~Device() = default;

            virtual std::unique_ptr<Buffer>   CreateBuffer(BufferType type, std::size_t bytes,
                                                           const void* initial, bool dynamic) = 0;
            virtual std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc) = 0;

            virtual void BindPipeline(Pipeline& pipeline) = 0;
            virtual void BindVertexBuffer(uint32_t binding, Buffer& buffer) = 0;
            virtual void BindUniformBuffer(uint32_t slot, Buffer& buffer) = 0;
            virtual void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount) = 0;

            // ---- Textures / render targets / fullscreen passes -----------------
            //  These have default no-op/null implementations so backends can adopt
            //  them incrementally (the WebGPU scaffold and any partial backend still
            //  compile). The OpenGL backend implements them.
            virtual std::unique_ptr<Texture>      CreateTexture(const TextureDesc&) { return nullptr; }
            virtual std::unique_ptr<RenderTarget> CreateRenderTarget(const RenderTargetDesc&) { return nullptr; }

            // Begin drawing into a target (nullptr = the default framebuffer). Sets
            // the viewport to the target's size; optionally clears. EndRenderPass
            // closes it. Draw() issues a non-instanced draw (e.g. a fullscreen tri).
            virtual void BeginRenderPass(RenderTarget* target, const RenderPassDesc& = {}) { (void)target; }
            virtual void EndRenderPass() {}
            virtual void BindTexture(uint32_t slot, Texture& texture) { (void)slot; (void)texture; }
            virtual void Draw(uint32_t vertexCount) { (void)vertexCount; }

            virtual const char* BackendName() const = 0;
        };

        // Creates the device for the backend selected at build time
        // (KE_BACKEND_* -> see RHI/GraphicsAPI.hpp). Defaults to OpenGL.
        std::unique_ptr<Device> CreateDevice();
    }
}
