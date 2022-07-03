#include "Mesh.h"

#include <cstring>

Mesh::Mesh()
{
}

Mesh::Mesh(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
    const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const glm::mat4& parentTransform,
    uint32_t materialId)
{
    m_device = device;
    m_physicalDevice = physicalDevice;

    m_transform = parentTransform;
    m_materialIndex = materialId;
    
    indices.empty() ? m_indexed = false : m_indexed = true;

    createVertexBuffer(transferQueue, transferCommandPool, vertices);
    m_vertexCount = static_cast<int>(vertices.size());

    if (m_indexed)
    {
        createIndexBuffer(transferQueue, transferCommandPool, indices);
        m_indexCount = static_cast<int>(indices.size());
    }
}

Mesh::~Mesh()
{
}

void Mesh::Destroy()
{
    m_indexBuffer.Destroy(m_device);
    m_vertexBuffer.Destroy(m_device);
}

int Mesh::GetVertexCount()
{
    return m_vertexCount;
}

Buffer* Mesh::GetVertexBuffer()
{
    return &m_vertexBuffer;
}

bool Mesh::Indexed()
{
    return m_indexed;
}

int Mesh::GetIndexCount()
{
    return m_indexCount;
}

Buffer* Mesh::GetIndexBuffer()
{
    return &m_indexBuffer;
}

const glm::mat4& Mesh::GetTransform()
{
    return m_transform;
}

uint32_t Mesh::GetMaterialIndex()
{
    return m_materialIndex;
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
                              const std::vector<Vertex>& vertices)
{
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    Buffer stagingBuffer;
    stagingBuffer.Init(m_device, m_physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_device, stagingBuffer.GetMemory(), 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(m_device, stagingBuffer.GetMemory());

    m_vertexBuffer.Init(m_device, m_physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer::CopyBuffer(m_device, transferQueue, transferCommandPool,
        stagingBuffer.GetBuffer(), m_vertexBuffer.GetBuffer(),
        bufferSize);

    stagingBuffer.Destroy(m_device);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
    const std::vector<uint32_t>& indices)
{
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    Buffer stagingBuffer;
    stagingBuffer.Init(m_device, m_physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_device, stagingBuffer.GetMemory(), 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(m_device, stagingBuffer.GetMemory());

    m_indexBuffer.Init(m_device, m_physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer::CopyBuffer(m_device, transferQueue, transferCommandPool,
        stagingBuffer.GetBuffer(), m_indexBuffer.GetBuffer(),
        bufferSize);
    
    stagingBuffer.Destroy(m_device);
}