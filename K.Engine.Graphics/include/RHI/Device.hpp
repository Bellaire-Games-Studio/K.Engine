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

            virtual const char* BackendName() const = 0;
        };

        // Creates the device for the backend selected at build time
        // (KE_BACKEND_* -> see RHI/GraphicsAPI.hpp). Defaults to OpenGL.
        std::unique_ptr<Device> CreateDevice();
    }
}
