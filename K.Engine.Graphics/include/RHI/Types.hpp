#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace KDot
{
    namespace rhi
    {
        // -------------------------------------------------------------------------
        // RHI core types
        //
        //  Backend-agnostic descriptions of GPU resources. Deliberately modelled
        //  after the explicit (Vulkan/WebGPU) style - buffers, a pipeline built
        //  from shaders + a vertex layout + fixed-function state, and a uniform
        //  buffer for per-draw constants - so the same renderer code drives the
        //  OpenGL backend today and Vulkan/WebGPU later.
        // -------------------------------------------------------------------------

        enum class BufferType
        {
            Vertex,
            Index,
            Uniform
        };

        enum class AttribFormat
        {
            Float1,
            Float2,
            Float3,
            Float4,
            UInt1
        };

        enum class CullMode
        {
            None,
            Back,
            Front
        };

        enum class PrimitiveTopology
        {
            Triangles
        };

        // ---- Textures / render targets -----------------------------------------
        enum class TextureFormat
        {
            RGBA8,    // 8-bit LDR colour
            RGBA16F,  // half-float HDR colour (filterable on ES3/WebGL2)
            Depth24   // sampleable depth
        };

        enum class TextureFilter
        {
            Nearest,
            Linear
        };

        enum class TextureWrap
        {
            ClampToEdge,
            Repeat
        };

        struct TextureDesc
        {
            uint32_t      width = 0;
            uint32_t      height = 0;
            TextureFormat format = TextureFormat::RGBA8;
            TextureFilter filter = TextureFilter::Linear;
            TextureWrap   wrap = TextureWrap::ClampToEdge;
            // Transitional: when non-zero, wrap an already-created backend texture
            // (e.g. a GL texture id owned elsewhere) instead of allocating one. The
            // RHI texture then borrows it and never frees it. Used while the renderer
            // is migrated onto the RHI pass by pass; removed once ownership moves over.
            uint64_t      externalHandle = 0;
        };

        struct VertexAttribute
        {
            uint32_t     location; // shader attribute location
            uint32_t     binding;  // which vertex binding it reads from
            AttribFormat format;
            uint32_t     offset;   // byte offset within the binding's stride
        };

        struct VertexBinding
        {
            uint32_t binding;
            uint32_t stride;
            bool     perInstance = false; // false = per-vertex, true = per-instance
        };

        struct VertexLayout
        {
            std::vector<VertexBinding>   bindings;
            std::vector<VertexAttribute> attributes;
        };

        struct PipelineDesc
        {
            std::string       vertexShaderPath;
            std::string       fragmentShaderPath;
            // Inline GLES-3.00 source. When non-empty these are compiled directly
            // instead of reading the *Path files - used for the small generated
            // post-process shaders that live in the renderer (and for any backend
            // that prefers in-memory source over a file).
            std::string       vertexSource;
            std::string       fragmentSource;
            VertexLayout      layout;
            PrimitiveTopology topology = PrimitiveTopology::Triangles;
            bool              depthTest = true;
            bool              depthWrite = true;
            bool              blend = false;
            CullMode          cull = CullMode::None;
            // Name of the std140 uniform block that receives per-draw constants
            // (bound to uniform slot 0). Empty = no constants block.
            std::string       constantsBlock = "Constants";
        };

        inline uint32_t AttribComponentCount(AttribFormat f)
        {
            switch (f)
            {
                case AttribFormat::Float1:
                case AttribFormat::UInt1:  return 1;
                case AttribFormat::Float2: return 2;
                case AttribFormat::Float3: return 3;
                case AttribFormat::Float4: return 4;
            }
            return 0;
        }

        inline bool AttribIsInteger(AttribFormat f) { return f == AttribFormat::UInt1; }
    }
}
