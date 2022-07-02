#include "Camera.h"

Camera::Camera()
{
    m_position = {0.f, 0.f, 0.f};
    m_pitch = 0.f;
    m_yaw = 0.f;

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

void Camera::AddPositionOffset(const glm::vec3& offset)
{
    AddPositionOffset(offset.x, offset.y, offset.z);
}

void Camera::AddRotation(float roll, float pitch, float yaw)
{
    m_pitch += pitch;
    m_yaw += yaw;
    
    onPosUpdate();
}

void Camera::SetProjection(float fov, float aspectRatio, float near, float far)
{
    m_projectionMatrix = glm::perspectiveZO(glm::radians(fov), aspectRatio, near, far);
}

const glm::vec3& Camera::GetPosition()
{
    return m_position;
}

const glm::vec3& Camera::GetRotation()
{
    return { m_pitch, m_yaw, 0.f };
}

void Camera::SetPosition(const glm::vec3& newPos)
{
    m_position = newPos;
    onPosUpdate();
}

void Camera::SetRotation(const glm::vec3& newRot)
{
    
    onPosUpdate();
}

glm::mat4 Camera::GetViewMatrix()
{
    return m_viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix()
{
    return m_projectionMatrix;
}

const glm::vec3& Camera::GetForwardVector()
{
    return m_forward;
}

const glm::vec3& Camera::GetUpVector()
{
    return m_up;
}

void Camera::onPosUpdate()
{
    if (m_pitch > 89.f) m_pitch = 89.f;
    if (m_pitch < -89.f) m_pitch = -89.f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    direction.y = sin(glm::radians(m_pitch));
    direction.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_forward = glm::normalize(direction);

    glm::vec3 lookAt = m_forward + m_position;
    m_viewMatrix = glm::lookAt(m_position, lookAt, m_up);
}
