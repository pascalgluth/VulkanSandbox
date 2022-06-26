#pragma once

#include <vulkan/vulkan.h>

class Buffer
{
public:
    Buffer();
    ~Buffer();

    void Init(VkDevice device, VkPhysicalDevice physicalDevice,
        VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
        VkMemoryPropertyFlags memoryProperties);
    void Destroy(VkDevice device);

    VkBuffer GetBuffer();
    VkDeviceMemory GetMemory();

    static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
        VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
        VkMemoryPropertyFlags memoryProperties,
        VkBuffer* buffer, VkDeviceMemory* memory);

    static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
        VkBuffer source, VkBuffer destination, VkDeviceSize size);

private:
    VkBuffer m_buffer;
    VkDeviceMemory m_bufferMemory;
    
};
