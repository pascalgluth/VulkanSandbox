#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

enum class ETextureType { DIFFUSE, NORMAL, SPECULAR };

struct Material
{
    uint32_t diffuse;
    uint32_t specular;
    uint32_t normal;
};

class MaterialManager
{
public:
    static void Init(VkDevice _device, VkPhysicalDevice _physicalDevice);
    static void Destroy();
    
    static VkDescriptorSetLayout GetDescriptorSetLayout();
    static VkDescriptorSet GetDescriptorSet(uint32_t materialId);
    static ETextureType GetTextureType(uint32_t textureId);

    static Material GetMaterial(uint32_t materialId);
    
    static uint32_t CreateMaterial(const std::string& diffuse, const std::string& specular, const std::string& normal, VkQueue queue, VkCommandPool commandPool);

private:
    // List of textures
    
};
