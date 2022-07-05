#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>

struct UboViewProjection
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 camPosition;
    glm::mat4 lightSpace;
    glm::mat4 spotLightSpace;
};

class Camera
{
public:
    Camera();
    ~Camera();

    void AddPositionOffset(float x, float y, float z);
    void AddPositionOffset(const glm::vec3& offset);
    void AddRotation(float roll, float pitch, float yaw);

    void SetProjection(float fov, float aspectRatio, float near, float far);

    const glm::vec3& GetPosition();
    glm::vec3 GetRotation();

    void SetPosition(const glm::vec3& newPos);
    void SetRotation(const glm::vec3& newRot);

    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix();

    const glm::vec3& GetForwardVector();
    const glm::vec3& GetUpVector();

private:
    glm::vec3 m_position;
    float m_pitch, m_yaw;

    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;

    const glm::vec3 DEFAULT_FORWARD = { 0.f, 0.f, -1.f };
    const glm::vec3 DEFAULT_UP = { 0.f, 1.f, 0.f };

    glm::vec3 m_forward = DEFAULT_FORWARD;
    glm::vec3 m_up = DEFAULT_UP;

    void onPosUpdate();
    
};
