#pragma once
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>

namespace KDot
{
    // A standard TRS transform component. Rotation is a quaternion to avoid
    // gimbal lock; helpers are provided for the common Euler-degrees case.
    struct Transform
    {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // identity (w,x,y,z)
        glm::vec3 scale{1.0f};

        Transform() = default;
        explicit Transform(const glm::vec3& pos) : position(pos) {}
        Transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl)
            : position(pos), rotation(rot), scale(scl) {}

        glm::mat4 Matrix() const
        {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
            m *= glm::mat4_cast(rotation);
            m = glm::scale(m, scale);
            return m;
        }

        glm::vec3 Forward() const { return rotation * glm::vec3(0.0f, 0.0f, -1.0f); }
        glm::vec3 Right()   const { return rotation * glm::vec3(1.0f, 0.0f, 0.0f); }
        glm::vec3 Up()      const { return rotation * glm::vec3(0.0f, 1.0f, 0.0f); }

        void SetEulerDegrees(const glm::vec3& eulerDeg)
        {
            rotation = glm::quat(glm::radians(eulerDeg));
        }
    };
}
