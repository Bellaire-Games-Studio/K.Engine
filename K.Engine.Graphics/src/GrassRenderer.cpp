#include <GrassRenderer.hpp>
#include <RHI/Types.hpp>
#include <cstddef>

namespace KDot
{
    namespace
    {
        // std140 layout matching the "Constants" block in the grass shaders.
        // All members are 16-byte aligned (mat4 / vec4) so the C++ struct maps
        // directly with no extra padding.
        struct GrassConstants
        {
            glm::mat4 projection;
            glm::mat4 view;
            glm::vec4 cameraPos;    // xyz
            glm::vec4 sunDir;       // xyz
            glm::vec4 sunColor;     // xyz
            glm::vec4 ambientColor; // xyz
            glm::vec4 fogColor;     // xyz
            glm::vec4 params0;      // time, bladeWidth, bladeHeight, maxDist
            glm::vec4 params1;      // ambientIntensity, sunIntensity, fogDensity, _
        };
    }

    bool GrassRenderer::Init()
    {
        m_Device = rhi::CreateDevice();
        if (!m_Device)
            return false;

        // One tapered blade quad (2 triangles): vBlade = (xSign, heightT).
        static const float blade[12] = {
            -1.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            -1.0f, 0.0f,  1.0f, 1.0f, -1.0f, 1.0f};

        m_BladeBuf    = m_Device->CreateBuffer(rhi::BufferType::Vertex, sizeof(blade), blade, false);
        m_InstanceBuf = m_Device->CreateBuffer(rhi::BufferType::Vertex, 0, nullptr, true);
        m_Ubo         = m_Device->CreateBuffer(rhi::BufferType::Uniform, sizeof(GrassConstants), nullptr, true);

        rhi::PipelineDesc desc;
        desc.vertexShaderPath   = "Assets/Shaders/3D/GrassVertex.glsl";
        desc.fragmentShaderPath = "Assets/Shaders/3D/GrassFragment.glsl";
        desc.depthTest = true;
        desc.depthWrite = true;
        desc.blend = false;
        desc.cull = rhi::CullMode::None; // billboards are viewed from both sides
        desc.constantsBlock = "Constants";

        // binding 0: per-vertex blade geometry (vec2). binding 1: per-instance data.
        desc.layout.bindings = {
            {0, 2 * sizeof(float), false},
            {1, sizeof(GrassInstance), true}};
        desc.layout.attributes = {
            {0, 0, rhi::AttribFormat::Float2, 0},
            {1, 1, rhi::AttribFormat::Float3, (uint32_t)offsetof(GrassInstance, position)},
            {2, 1, rhi::AttribFormat::Float3, (uint32_t)offsetof(GrassInstance, phase)}};

        m_Pipeline = m_Device->CreatePipeline(desc);
        return m_Pipeline && m_Pipeline->Valid();
    }

    void GrassRenderer::SetInstances(const std::vector<GrassInstance>& instances)
    {
        m_Count = static_cast<int>(instances.size());
        if (m_InstanceBuf && !instances.empty())
            m_InstanceBuf->Update(instances.data(), instances.size() * sizeof(GrassInstance));
    }

    void GrassRenderer::Render(const glm::mat4& view, const glm::mat4& proj,
                               const glm::vec3& cameraPos, float time, const LightManager& lights)
    {
        if (m_Count <= 0 || !m_Pipeline || !m_Pipeline->Valid())
            return;

        GrassConstants c{};
        c.projection   = proj;
        c.view         = view;
        c.cameraPos    = glm::vec4(cameraPos, 1.0f);
        c.sunDir       = glm::vec4(glm::normalize(lights.sun.direction), 0.0f);
        c.sunColor     = glm::vec4(lights.sun.color, 0.0f);
        c.ambientColor = glm::vec4(lights.ambient.color, 0.0f);
        c.fogColor     = glm::vec4(lights.fogColor, 0.0f);
        c.params0      = glm::vec4(time, bladeWidth, bladeHeight, maxDistance);
        c.params1      = glm::vec4(lights.ambient.intensity, lights.sun.intensity, lights.fogDensity, 0.0f);

        m_Ubo->Update(&c, sizeof(c));

        m_Device->BindPipeline(*m_Pipeline);
        m_Device->BindVertexBuffer(0, *m_BladeBuf);
        m_Device->BindVertexBuffer(1, *m_InstanceBuf);
        m_Device->BindUniformBuffer(0, *m_Ubo);
        m_Device->DrawInstanced(6, static_cast<uint32_t>(m_Count));
    }
}
