#include "Camera.h"

Camera::Camera()
{
    m_position = {0.f, 0.f, 0.f};
    m_rotation = {0.f, 0.f, 0.f};

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
}

const glm::vec3& Camera::GetPosition()
{
    return m_position;
}

const glm::vec3& Camera::GetRotation()
{
    return m_rotation;
}

void Camera::SetPosition(const glm::vec3& newPos)
{
    m_position = newPos;
    onPosUpdate();
}

void Camera::SetRotation(const glm::vec3& newRot)
{
    m_rotation = newRot;
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

void Camera::onPosUpdate()
{
    m_forward = glm::rotate(DEFAULT_FORWARD, glm::radians(m_rotation.x), {1.f, 0.f, 0.f}) +
        glm::rotate(DEFAULT_FORWARD, glm::radians(m_rotation.y), {0.f, 1.f, 0.f});

    m_up = glm::rotate(DEFAULT_UP, glm::radians(m_rotation.x), {1.f, 0.f, 0.f}) +
        glm::rotate(DEFAULT_UP, glm::radians(m_rotation.y), {0.f, 1.f, 0.f}) +
        glm::rotate(DEFAULT_UP, glm::radians(m_rotation.z), {0.f, 0.f, 1.f});

    glm::vec3 lookAt = m_forward + m_position;
    m_viewMatrix = glm::lookAt(m_position, lookAt, m_up);
}
