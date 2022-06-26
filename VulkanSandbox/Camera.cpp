#include "Camera.h"

Camera::Camera()
{
    m_position = { 0.f, 0.f, 0.f };
    m_rotation = { 0.f, 0.f, 0.f };

    onPosUpdate();
}

Camera::~Camera()
{
}

void Camera::AddPositionOffset(float x, float y, float z)
{
    m_position.x += x;
    m_position.y += y;
    m_position.z += z;
    onPosUpdate();
}

void Camera::AddRotation(float roll, float pitch, float yaw)
{
    m_rotation.x += roll;
    m_rotation.y += pitch;
    m_rotation.z += yaw;
    onPosUpdate();
}

void Camera::SetProjection(float fov, float aspectRatio, float near, float far)
{
    m_projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, near, far);
    m_projectionMatrix[1][1] *= -1;
}

glm::mat4 Camera::GetViewMatrix()
{
    return m_viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix()
{
    return m_projectionMatrix;
}

void Camera::onPosUpdate()
{
    glm::vec3 forward;
   // m_forward *= glm::eulerAngles(glm::qua<float>(m_rotation));
    
    m_viewMatrix = glm::lookAt(m_position, m_position + m_forward, m_up);
}
