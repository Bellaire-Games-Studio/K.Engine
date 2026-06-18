// -----------------------------------------------------------------------------
// Experimental WebGPU backend (KE_BACKEND_WEBGPU) - see WebGPUDevice.hpp and
// docs/WEBGPU_PORT.md. Compiled only under the WebGPU build option; the default
// OpenGL build sees an empty translation unit.
//
//  Status: SCAFFOLD. The buffer/shader/pipeline mapping is implemented against
//  Emscripten's webgpu.h, but it is not yet driven by a running frame loop (the
//  rest of the engine is still OpenGL). The WebGPU C API + WGSL evolve; some
//  descriptor field names may need to track the Emscripten SDK version in use.
// -----------------------------------------------------------------------------
#if defined(KE_BACKEND_WEBGPU)

#include <RHI/WebGPUDevice.hpp>
#include <emscripten/html5_webgpu.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace KDot
{
    namespace rhi
    {
        namespace
        {
            std::size_t RoundUp4(std::size_t n) { return (n + 3u) & ~std::size_t(3u); }

            WGPUBufferUsageFlags UsageFor(BufferType t)
            {
                switch (t)
                {
                    case BufferType::Vertex:  return WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
                    case BufferType::Index:   return WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
                    case BufferType::Uniform: return WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
                }
                return WGPUBufferUsage_CopyDst;
            }

            WGPUVertexFormat VertexFormatFor(AttribFormat f)
            {
                switch (f)
                {
                    case AttribFormat::Float1: return WGPUVertexFormat_Float32;
                    case AttribFormat::Float2: return WGPUVertexFormat_Float32x2;
                    case AttribFormat::Float3: return WGPUVertexFormat_Float32x3;
                    case AttribFormat::Float4: return WGPUVertexFormat_Float32x4;
                    case AttribFormat::UInt1:  return WGPUVertexFormat_Uint32;
                }
                return WGPUVertexFormat_Float32;
            }

            // Each GLSL vert/frag pair has a sibling combined WGSL module
            // (GrassVertex.glsl + GrassFragment.glsl -> Grass.wgsl).
            std::string WgslPathFor(const std::string& glslVertPath)
            {
                const std::string marker = "Vertex.glsl";
                const std::size_t at = glslVertPath.rfind(marker);
                if (at != std::string::npos)
                    return glslVertPath.substr(0, at) + ".wgsl";
                const std::size_t dot = glslVertPath.rfind(".glsl");
                return (dot == std::string::npos) ? glslVertPath : glslVertPath.substr(0, dot) + ".wgsl";
            }

            std::string ReadFile(const std::string& path)
            {
                std::ifstream in(path, std::ios::in);
                if (!in.is_open())
                {
                    std::cout << "WebGPU: failed to open shader " << path << std::endl;
                    return {};
                }
                return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            }

            WGPUShaderModule CreateWgslModule(WGPUDevice device, const std::string& src)
            {
                if (src.empty())
                    return nullptr;
                WGPUShaderModuleWGSLDescriptor wgsl = {};
                wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
                wgsl.code = src.c_str(); // NB: newer webgpu.h may use WGPUStringView here
                WGPUShaderModuleDescriptor smd = {};
                smd.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl);
                return wgpuDeviceCreateShaderModule(device, &smd);
            }
        }

        // ---- WGPUBufferImpl ----------------------------------------------------
        WGPUBufferImpl::WGPUBufferImpl(WGPUDevice device, WGPUQueue queue, WGPUBufferUsageFlags usage,
                                       std::size_t bytes, const void* initial)
            : m_Device(device), m_Queue(queue), m_Size(RoundUp4(bytes))
        {
            WGPUBufferDescriptor bd = {};
            bd.usage = usage;
            bd.size = m_Size;
            bd.mappedAtCreation = false;
            m_Buffer = wgpuDeviceCreateBuffer(device, &bd);
            if (initial && bytes > 0)
                wgpuQueueWriteBuffer(queue, m_Buffer, 0, initial, RoundUp4(bytes));
        }

        WGPUBufferImpl::~WGPUBufferImpl()
        {
            if (m_Buffer)
                wgpuBufferRelease(m_Buffer);
        }

        void WGPUBufferImpl::Update(const void* data, std::size_t bytes)
        {
            if (!m_Buffer || !data || bytes == 0)
                return;
            // Note: a growing dynamic buffer would need re-creation here; the
            // grass instance buffer is sized up front in the GL path.
            const std::size_t n = RoundUp4(bytes);
            if (n <= m_Size)
                wgpuQueueWriteBuffer(m_Queue, m_Buffer, 0, data, n);
        }

        // ---- WGPUPipelineImpl --------------------------------------------------
        WGPUPipelineImpl::WGPUPipelineImpl(WGPUDevice device, const PipelineDesc& desc,
                                           WGPUTextureFormat colorFormat)
        {
            const std::string src = ReadFile(WgslPathFor(desc.vertexShaderPath));
            WGPUShaderModule module = CreateWgslModule(device, src);
            if (!module)
                return;

            // Vertex buffer layouts (assumes binding indices are 0..N-1).
            std::vector<std::vector<WGPUVertexAttribute>> attrs(desc.layout.bindings.size());
            for (const VertexAttribute& a : desc.layout.attributes)
            {
                if (a.binding >= attrs.size())
                    continue;
                WGPUVertexAttribute wa = {};
                wa.format = VertexFormatFor(a.format);
                wa.offset = a.offset;
                wa.shaderLocation = a.location;
                attrs[a.binding].push_back(wa);
            }
            std::vector<WGPUVertexBufferLayout> buffers(desc.layout.bindings.size());
            for (const VertexBinding& b : desc.layout.bindings)
            {
                if (b.binding >= buffers.size())
                    continue;
                WGPUVertexBufferLayout vbl = {};
                vbl.arrayStride = b.stride;
                vbl.stepMode = b.perInstance ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex;
                vbl.attributeCount = attrs[b.binding].size();
                vbl.attributes = attrs[b.binding].data();
                buffers[b.binding] = vbl;
            }

            // Bind group layout: the per-draw uniform buffer at group 0, binding 0.
            WGPUBindGroupLayoutEntry bglEntry = {};
            bglEntry.binding = 0;
            bglEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
            bglEntry.buffer.type = WGPUBufferBindingType_Uniform;
            WGPUBindGroupLayoutDescriptor bglDesc = {};
            bglDesc.entryCount = 1;
            bglDesc.entries = &bglEntry;
            m_BindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

            WGPUPipelineLayoutDescriptor plDesc = {};
            plDesc.bindGroupLayoutCount = 1;
            plDesc.bindGroupLayouts = &m_BindGroupLayout;
            WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

            // Colour target (+ optional alpha blend).
            WGPUBlendState blend = {};
            blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
            blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
            blend.color.operation = WGPUBlendOperation_Add;
            blend.alpha.srcFactor = WGPUBlendFactor_One;
            blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
            blend.alpha.operation = WGPUBlendOperation_Add;

            WGPUColorTargetState colorTarget = {};
            colorTarget.format = colorFormat;
            colorTarget.blend = desc.blend ? &blend : nullptr;
            colorTarget.writeMask = WGPUColorWriteMask_All;

            WGPUFragmentState fragment = {};
            fragment.module = module;
            fragment.entryPoint = "fs_main";
            fragment.targetCount = 1;
            fragment.targets = &colorTarget;

            // Depth state mirrors the GL pipeline desc.
            WGPUDepthStencilState depth = {};
            depth.format = WGPUTextureFormat_Depth24Plus;
            depth.depthWriteEnabled = desc.depthWrite;
            depth.depthCompare = desc.depthTest ? WGPUCompareFunction_Less : WGPUCompareFunction_Always;

            WGPURenderPipelineDescriptor pd = {};
            pd.layout = layout;
            pd.vertex.module = module;
            pd.vertex.entryPoint = "vs_main";
            pd.vertex.bufferCount = buffers.size();
            pd.vertex.buffers = buffers.data();
            pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
            pd.primitive.frontFace = WGPUFrontFace_CCW;
            switch (desc.cull)
            {
                case CullMode::None:  pd.primitive.cullMode = WGPUCullMode_None;  break;
                case CullMode::Back:  pd.primitive.cullMode = WGPUCullMode_Back;  break;
                case CullMode::Front: pd.primitive.cullMode = WGPUCullMode_Front; break;
            }
            pd.depthStencil = (desc.depthTest || desc.depthWrite) ? &depth : nullptr;
            pd.multisample.count = 1;
            pd.multisample.mask = 0xFFFFFFFFu;
            pd.fragment = &fragment;

            m_Pipeline = wgpuDeviceCreateRenderPipeline(device, &pd);

            if (layout) wgpuPipelineLayoutRelease(layout);
            if (module) wgpuShaderModuleRelease(module);
        }

        WGPUPipelineImpl::~WGPUPipelineImpl()
        {
            if (m_Pipeline)        wgpuRenderPipelineRelease(m_Pipeline);
            if (m_BindGroupLayout) wgpuBindGroupLayoutRelease(m_BindGroupLayout);
        }

        // ---- WebGPUDevice ------------------------------------------------------
        WebGPUDevice::WebGPUDevice()
        {
            // Emscripten hands us the device that was created in JS / the shell
            // (or via the async adapter/device request the full port will own).
            m_Device = emscripten_webgpu_get_device();
            if (m_Device)
                m_Queue = wgpuDeviceGetQueue(m_Device);
            else
                std::cout << "WebGPU: no device (emscripten_webgpu_get_device returned null)" << std::endl;
        }

        WebGPUDevice::~WebGPUDevice()
        {
            if (m_Queue)  wgpuQueueRelease(m_Queue);
            if (m_Device) wgpuDeviceRelease(m_Device);
        }

        std::unique_ptr<Buffer> WebGPUDevice::CreateBuffer(BufferType type, std::size_t bytes,
                                                           const void* initial, bool /*dynamic*/)
        {
            return std::make_unique<WGPUBufferImpl>(m_Device, m_Queue, UsageFor(type),
                                                    bytes == 0 ? 4u : bytes, initial);
        }

        std::unique_ptr<Pipeline> WebGPUDevice::CreatePipeline(const PipelineDesc& desc)
        {
            return std::make_unique<WGPUPipelineImpl>(m_Device, desc, m_ColorFormat);
        }

        void WebGPUDevice::BindPipeline(Pipeline& pipeline)
        {
            m_CurrentPipeline = static_cast<WGPUPipelineImpl*>(&pipeline);
            if (m_Pass)
                wgpuRenderPassEncoderSetPipeline(m_Pass, m_CurrentPipeline->Handle());
        }

        void WebGPUDevice::BindVertexBuffer(uint32_t binding, Buffer& buffer)
        {
            if (!m_Pass)
                return;
            WGPUBufferImpl& b = static_cast<WGPUBufferImpl&>(buffer);
            wgpuRenderPassEncoderSetVertexBuffer(m_Pass, binding, b.Handle(), 0, b.Size());
        }

        void WebGPUDevice::BindUniformBuffer(uint32_t slot, Buffer& buffer)
        {
            if (!m_Pass || !m_CurrentPipeline)
                return;
            WGPUBufferImpl& b = static_cast<WGPUBufferImpl&>(buffer);

            WGPUBindGroupEntry entry = {};
            entry.binding = 0;
            entry.buffer = b.Handle();
            entry.offset = 0;
            entry.size = b.Size();
            WGPUBindGroupDescriptor bgDesc = {};
            bgDesc.layout = m_CurrentPipeline->BindGroupLayout();
            bgDesc.entryCount = 1;
            bgDesc.entries = &entry;
            // The bind group must outlive the pass; a real port should cache one
            // per (pipeline, buffer). This scaffold leaks it until EndFrame.
            WGPUBindGroup bg = wgpuDeviceCreateBindGroup(m_Device, &bgDesc);
            wgpuRenderPassEncoderSetBindGroup(m_Pass, slot, bg, 0, nullptr);
        }

        void WebGPUDevice::DrawInstanced(uint32_t vertexCount, uint32_t instanceCount)
        {
            if (m_Pass && instanceCount > 0)
                wgpuRenderPassEncoderDraw(m_Pass, vertexCount, instanceCount, 0, 0);
        }

        void WebGPUDevice::BeginFrame(WGPUTextureView target)
        {
            if (!m_Device || !target)
                return;
            m_Encoder = wgpuDeviceCreateCommandEncoder(m_Device, nullptr);

            WGPURenderPassColorAttachment color = {};
            color.view = target;
            color.loadOp = WGPULoadOp_Clear;
            color.storeOp = WGPUStoreOp_Store;
            color.clearValue = WGPUColor{0.45, 0.62, 0.85, 1.0};

            WGPURenderPassDescriptor rp = {};
            rp.colorAttachmentCount = 1;
            rp.colorAttachments = &color;
            // A real frame also attaches a depth texture here (Depth24Plus).
            m_Pass = wgpuCommandEncoderBeginRenderPass(m_Encoder, &rp);
        }

        void WebGPUDevice::EndFrame()
        {
            if (!m_Pass)
                return;
            wgpuRenderPassEncoderEnd(m_Pass); // older SDKs: wgpuRenderPassEncoderEndPass
            WGPUCommandBuffer cb = wgpuCommandEncoderFinish(m_Encoder, nullptr);
            wgpuQueueSubmit(m_Queue, 1, &cb);

            wgpuCommandBufferRelease(cb);
            wgpuRenderPassEncoderRelease(m_Pass);
            wgpuCommandEncoderRelease(m_Encoder);
            m_Pass = nullptr;
            m_Encoder = nullptr;
        }

        std::unique_ptr<Device> CreateWebGPUDevice()
        {
            return std::make_unique<WebGPUDevice>();
        }
    }
}

#endif // KE_BACKEND_WEBGPU
