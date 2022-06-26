#pragma once

#include <vulkan/vulkan.h>

class Image
{
public:
    Image();
    ~Image();

    void Init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format,
              VkSampleCountFlagBits samples, VkImageTiling tiling,
              VkImageUsageFlags useFlags, VkMemoryPropertyFlags memoryFlags, VkImageAspectFlags aspectFlags);
    void Destroy(VkDevice device);

    static VkImage CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                               VkFormat format, VkSampleCountFlagBits samples, VkImageTiling tiling,
                               VkImageUsageFlags useFlags, VkMemoryPropertyFlags memoryFlags,
                               VkDeviceMemory* imageMemory);
    static VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    VkImage GetImage();
    VkDeviceMemory GetMemory();
    VkImageView GetImageView();

private:
    VkImage m_image;
    VkDeviceMemory m_memory;
    VkImageView m_imageView;
};
