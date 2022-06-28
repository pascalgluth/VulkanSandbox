#pragma once

#include <vector>
#include <assimp/scene.h>

#include "Mesh.h"

class Object
{
public:
    Object();
    ~Object();

    void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::string& modelFile);
    void Update(float deltaTime);
    void Destroy();

    std::vector<Mesh>& GetMeshes();

    const glm::mat4& GetTransform();

    const glm::vec3& GetPosition();
    void SetPosition(const glm::vec3& newPos);

private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;

    glm::vec3 m_position;
    glm::mat4 m_transform;
    
    std::vector<Mesh> m_meshes;

    std::vector<Mesh> LoadNode(VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node, const aiScene* scene, const glm::mat4 parentTransform);
    Mesh LoadMesh(VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh, const aiScene* scene, const glm::mat4 parentTransform);
    
};
