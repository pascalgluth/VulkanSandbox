#pragma once

#include <vector>
#include <assimp/scene.h>

#include "Mesh.h"

class Object
{
public:
    Object(const std::string& name);
    ~Object();

    void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::string& modelFile);
    void Update(float deltaTime);
    void Destroy();

    std::vector<Mesh>& GetMeshes();

    const glm::mat4& GetTransform();

    const glm::vec3& GetPosition();
    void SetPosition(const glm::vec3& newPos);

    void SetScale(const glm::vec3& scale);

    std::string Name;

    uint32_t GetMaterialId(uint32_t index);
    
private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;

    glm::vec3 m_scale;
    glm::vec3 m_position;
    glm::mat4 m_transform;

    void matrixUpdate();

    std::vector<uint32_t> m_materialIndices;
    std::vector<Mesh> m_meshes;

    std::vector<Mesh> LoadNode(VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node, const aiScene* scene, const glm::mat4 parentTransform);
    Mesh LoadMesh(VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh, const aiScene* scene, const glm::mat4 parentTransform);
    
};
