#include <glm.hpp>
#include <ext.hpp>
#include <stdio.h>
namespace KDot
{
    class Camera
    {
    public:
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);
        virtual ~Camera() = default;
        void Update();
        void Update(const double deltaTime);
        glm::mat4 GetViewMatrix() const { return glm::lookAt(m_Position, m_Position + m_Front, m_Up); }
        float getZoom() const { return zoom; }
        glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f);

    protected:
        float zoom = 45.0f;
        float movementSpeed = 0.003f;
        float mouseSensitivity = 0.1f;
        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;

        glm::vec3 m_Right;
        glm::vec3 m_Up;
        glm::vec3 m_Front;
        glm::vec3 m_WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    };
}