#pragma once

#define GLM_FORCE_RADIANS
#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct PushViewProjection
{
    glm::mat4 view;
    glm::mat4 projection;
};

class Camera
{
public:
    Camera();
    ~Camera();

    void AddPositionOffset(float x, float y, float z);
    void AddRotation(float roll, float pitch, float yaw);

    void SetProjection(float fov, float aspectRatio, float near, float far);

    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix();

private:
    glm::vec3 m_position;
    glm::vec3 m_rotation;

    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;

    glm::vec3 m_forward = { 0.f, 0.f, -1.f };
    glm::vec3 m_up = { 0.f, 1.f, 0.f };

    void onPosUpdate();
    
};
