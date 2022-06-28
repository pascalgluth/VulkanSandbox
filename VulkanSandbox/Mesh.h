#pragma once

#include "Buffer.h"
#include "Utilities.h"

class Mesh
{
public:
    Mesh();
    Mesh(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
        const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const glm::mat4& parentTransform);
    ~Mesh();

    void Destroy();
    
    int GetVertexCount();
    Buffer* GetVertexBuffer();

    bool Indexed();
    int GetIndexCount();
    Buffer* GetIndexBuffer();

    const glm::mat4& GetTransform();

private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;

    glm::mat4 m_transform;
    
    int m_vertexCount;
    Buffer m_vertexBuffer;
    void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
        const std::vector<Vertex>& vertices);

    bool m_indexed;
    int m_indexCount;
    Buffer m_indexBuffer;
    void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
        const std::vector<uint32_t>& indices);
    
    
};
