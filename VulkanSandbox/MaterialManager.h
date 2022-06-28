#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

class MaterialManager
{
public:
    static void Init(VkDevice _device, VkPhysicalDevice _physicalDevice);
    static void Destroy();
    
    static VkDescriptorSetLayout GetDescriptorSetLayout();
    static VkDescriptorSet GetDescriptorSet(uint32_t textureId);
    
    static uint32_t CreateTexture(const std::string& fileName, VkQueue queue, VkCommandPool commandPool);

private:
    // List of textures
    
};
