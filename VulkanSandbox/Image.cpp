#include "Image.h"

#include "Utilities.h"

Image::Image()
{
}

Image::~Image()
{
}

void Image::Init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format,
                 VkSampleCountFlagBits samples,
                 VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags memoryFlags,
                 VkImageAspectFlags aspectFlags)
{
    m_image = CreateImage(device, physicalDevice, width, height, format, samples, tiling, useFlags, memoryFlags, &m_memory);
    m_imageView = CreateImageView(device, m_image, format, aspectFlags);
}

void Image::Destroy(VkDevice device)
{
    vkDestroyImageView(device, m_imageView, nullptr);
    vkFreeMemory(device, m_memory, nullptr);
    vkDestroyImage(device, m_image, nullptr);
}

VkImage Image::CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                           VkFormat format,
                           VkSampleCountFlagBits samples, VkImageTiling tiling, VkImageUsageFlags useFlags,
                           VkMemoryPropertyFlags memoryFlags,
                           VkDeviceMemory* imageMemory)
{
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = useFlags;
    imageCreateInfo.samples = samples;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
    CHECK_VK_RESULT(result, "Failed to create Image");

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits,
                                                          memoryFlags);

    result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, imageMemory);
    CHECK_VK_RESULT(result, "Failed to allocate memory for Image");

    result = vkBindImageMemory(device, image, *imageMemory, 0);
    CHECK_VK_RESULT(result, "Failed to bind memory to Image");

    return image;
}

VkImageView Image::CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
    CHECK_VK_RESULT(result, "Failed to create Image View");
    return imageView;
}

VkImage Image::GetImage()
{
    return m_image;
}

VkDeviceMemory Image::GetMemory()
{
    return m_memory;
}

VkImageView Image::GetImageView()
{
    return m_imageView;
}
