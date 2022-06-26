#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool,
    const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    m_device = device;
    m_physicalDevice = physicalDevice;

    createVertexBuffer(transferQueue, transferCommandPool, vertices);
    m_vertexCount = static_cast<int>(vertices.size());
}

Mesh::~Mesh()
{
}

void Mesh::Destroy()
{
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
