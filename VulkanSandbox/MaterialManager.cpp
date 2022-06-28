#include "MaterialManager.h"

#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Buffer.h"
#include "Image.h"
#include "Utilities.h"

VkDevice device;
VkPhysicalDevice physicalDevice;

VkSampler textureSampler;

std::vector<Image> textureImages;

VkDescriptorSetLayout samplerSetLayout;
VkDescriptorPool samplerDescriptorPool;

std::vector<VkDescriptorSet> samplerDescriptorSets;

void createSetLayout();
void createDescriptorPool();
void createSampler();

stbi_uc* loadTextureImageFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize);

void MaterialManager::Init(VkDevice _device, VkPhysicalDevice _physicalDevice)
{
    device = _device;
    physicalDevice = _physicalDevice;

    createSetLayout();
    createDescriptorPool();
    createSampler();
}

void MaterialManager::Destroy()
{
    for (size_t i = 0; i < textureImages.size(); ++i)
    {
        textureImages[i].Destroy(device);
    }

    vkDestroySampler(device, textureSampler, nullptr);
    
    vkDestroyDescriptorPool(device, samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, samplerSetLayout, nullptr);
}

VkDescriptorSetLayout MaterialManager::GetDescriptorSetLayout()
{
    return samplerSetLayout;
}

VkDescriptorSet MaterialManager::GetDescriptorSet(uint32_t textureId)
{
    if (textureId >= samplerDescriptorSets.size())
        throw std::runtime_error("Invalid Texture ID: " + std::to_string(textureId));

    return samplerDescriptorSets[textureId];
}

uint32_t MaterialManager::CreateTexture(const std::string& fileName, VkQueue queue, VkCommandPool commandPool)
{
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc* imageData = loadTextureImageFile(fileName, &width, &height, &imageSize);

    Buffer stagingBuffer;
    stagingBuffer.Init(device, physicalDevice, imageSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(device, stagingBuffer.GetMemory(), 0, imageSize, 0, &data);
    memcpy(data, imageData, imageSize);
    vkUnmapMemory(device, stagingBuffer.GetMemory());

    stbi_image_free(imageData);

    Image textureImage;
    textureImage.Init(device, physicalDevice, width, height,
                      VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    textureImage.TransitionLayout(device, queue, commandPool,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Buffer::CopyBufferToImage(device, queue, commandPool, stagingBuffer.GetBuffer(), textureImage.GetImage(), width, height);
    textureImage.TransitionLayout(device, queue, commandPool,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    textureImages.push_back(std::move(textureImage));
    stagingBuffer.Destroy(device);

    uint32_t id = static_cast<uint32_t>(textureImages.size())-1;
    
    VkDescriptorSet descriptorSet;

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = samplerDescriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    descriptorSetAllocInfo.pSetLayouts = &samplerSetLayout;

    VkResult result = vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet);
    CHECK_VK_RESULT(result, "Failed to allocate Descriptor Set for Image");

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImages[id].GetImageView();
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    samplerDescriptorSets.push_back(descriptorSet);

    if ((static_cast<uint32_t>(samplerDescriptorSets.size())-1) != id)
        throw std::runtime_error("WTF happened");
    
    return id;
}

void createSetLayout()
{
    VkDescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = 1;
    layoutCreateInfo.pBindings = &samplerLayoutBinding;

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &samplerSetLayout);
    CHECK_VK_RESULT(result, "Failed to create Sampler Descriptor Layout");
}

void createDescriptorPool()
{
    VkDescriptorPoolSize samplerPoolSize;
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = MAX_TEXTURE_COUNT;

    VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
    samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolCreateInfo.maxSets = MAX_TEXTURE_COUNT;
    samplerPoolCreateInfo.poolSizeCount = 1;
    samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

    VkResult result = vkCreateDescriptorPool(device, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
    CHECK_VK_RESULT(result, "Failed to create Sampler Descriptor Pool");
}

void createSampler()
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    std::cout << "Maximum Anisotropy Level: " << deviceProperties.limits.maxSamplerAnisotropy << std::endl;
    
    // Sampler creation info
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.f;
    samplerCreateInfo.minLod = 0.f;
    samplerCreateInfo.maxLod = 0.f;
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;

    VkResult result = vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureSampler);
    CHECK_VK_RESULT(result, "Failed to create Texture Sampler");
}


stbi_uc* loadTextureImageFile(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize)
{
    int channels;

    std::string fileLoc = "textures/" + fileName;
    stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);
    if (!image) throw std::runtime_error("Failed to load texture file: " + fileName);

    *imageSize = (*width) * (*height) * 4;

    return image;
}
