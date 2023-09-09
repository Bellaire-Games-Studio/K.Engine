#include <Camera.hpp>
#include <Input.hpp>
#include <Event/Events.hpp>

namespace KDot
{
    Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) : m_Front(glm::vec3(0.0f, 0.0f, -1.0f)), m_Position(position), m_WorldUp(up), m_Yaw(yaw), m_Pitch(pitch)
    {
        Update();
    }
    void Camera::Update()
    { // matrices
        glm::vec3 front;
        front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        front.y = sin(glm::radians(m_Pitch));
        front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        m_Front = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
        m_Up = glm::normalize(glm::cross(m_Right, m_Front));
    }
    void Camera::Update(const double deltaTime)
    { // Inputs
        if (Input::IsKeyPressed(KDot::Key::W))
            m_Position += m_Front * movementSpeed * static_cast<float>(deltaTime);
        if (Input::IsKeyPressed(KDot::Key::S))
            m_Position -= m_Front * movementSpeed * static_cast<float>(deltaTime);
        if (Input::IsKeyPressed(KDot::Key::A))
            m_Position -= m_Right * movementSpeed * static_cast<float>(deltaTime);
        if (Input::IsKeyPressed(KDot::Key::D))
            m_Position += m_Right * movementSpeed * static_cast<float>(deltaTime);
    }
}