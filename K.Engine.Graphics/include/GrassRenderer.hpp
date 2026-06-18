#pragma once
#include <vector>
#include <memory>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <Core/GrassInstance.hpp>
#include <Light.hpp>
#include <RHI/Device.hpp>

namespace KDot
{
    // GPU-instanced grass, now expressed entirely through the RHI (no direct GL):
    // one blade vertex buffer + a per-blade instance buffer + a constants UBO,
    // drawn in a single instanced call. Swapping the RHI backend (GL -> Vulkan)
    // renders the same grass with no changes here.
    class GrassRenderer
    {
    public:
        GrassRenderer() = default;
        ~GrassRenderer() = default;

        bool Init();
        void SetInstances(const std::vector<GrassInstance>& instances);
        void Render(const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& cameraPos, float time, const LightManager& lights);

        int InstanceCount() const { return m_Count; }

        float bladeWidth  = 0.16f;
        float bladeHeight = 1.6f;
        float maxDistance = 320.0f;

    private:
        std::unique_ptr<rhi::Device>   m_Device;
        std::unique_ptr<rhi::Buffer>   m_BladeBuf;
        std::unique_ptr<rhi::Buffer>   m_InstanceBuf;
        std::unique_ptr<rhi::Buffer>   m_Ubo;
        std::unique_ptr<rhi::Pipeline> m_Pipeline;
        int m_Count = 0;
        int m_BladeVerts = 6; // vertices per blade (set from the segment count in Init)
    };
}
