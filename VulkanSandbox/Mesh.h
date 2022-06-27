#pragma once

#include "Buffer.h"
#include "Utilities.h"

class Mesh
{
public:
    Mesh();
    Mesh(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
        const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();

    void Destroy();
    
    int GetVertexCount();
    Buffer* GetVertexBuffer();

    bool Indexed();
    int GetIndexCount();
    Buffer* GetIndexBuffer();

private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    
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
