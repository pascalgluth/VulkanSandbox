#include "Buffer.h"

#include "Utilities.h"

Buffer::Buffer()
{
}

Buffer::~Buffer()
{
}

void Buffer::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize bufferSize,
                  VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryProperties)
{
    CreateBuffer(device, physicalDevice, bufferSize, bufferUsage, memoryProperties, &m_buffer, &m_bufferMemory);
}

void Buffer::Destroy(VkDevice device)
{
    vkFreeMemory(device, m_bufferMemory, nullptr);
    vkDestroyBuffer(device, m_buffer, nullptr);
}

VkBuffer Buffer::GetBuffer()
{
    return m_buffer;
}

VkDeviceMemory Buffer::GetMemory()
{
    return m_bufferMemory;
}

void Buffer::CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize bufferSize,
                          VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryProperties,
                          VkBuffer* buffer, VkDeviceMemory* memory)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = bufferUsage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CHECK_VK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer), "Failed to create Buffer");

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits,
                                                          memoryProperties);

    CHECK_VK_RESULT(vkAllocateMemory(device, &memoryAllocInfo, nullptr, memory), "Failed to allocate Memory");
    CHECK_VK_RESULT(vkBindBufferMemory(device, *buffer, *memory, 0), "Failed to bind Memory to Buffer");
}

void Buffer::CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer source,
                        VkBuffer destination, VkDeviceSize size)
{
    VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    vkCmdCopyBuffer(transferCommandBuffer, source, destination, 1, &copyRegion);

    endCommandBufferAndSubmit(device, transferCommandPool, transferQueue, transferCommandBuffer);
}
