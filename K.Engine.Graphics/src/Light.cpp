#include <Light.hpp>
#include <algorithm>
#include <numeric>

namespace KDot
{
    std::vector<PointLight> LightManager::SelectPoints(const glm::vec3& around, int maxLights) const
    {
        if (maxLights <= 0 || points.empty())
            return {};

        // Sort indices by squared distance so we only copy the survivors.
        std::vector<std::size_t> order(points.size());
        std::iota(order.begin(), order.end(), 0);

        std::sort(order.begin(), order.end(),
                  [&](std::size_t a, std::size_t b)
                  {
                      const float da = glm::dot(points[a].position - around, points[a].position - around);
                      const float db = glm::dot(points[b].position - around, points[b].position - around);
                      return da < db;
                  });

        const std::size_t count = std::min<std::size_t>(order.size(), static_cast<std::size_t>(maxLights));
        std::vector<PointLight> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            result.push_back(points[order[i]]);
        return result;
    }
}
