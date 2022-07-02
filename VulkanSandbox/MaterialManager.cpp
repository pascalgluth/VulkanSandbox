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
VkSampler normalSampler;
VkSampler specularSampler;

VkDescriptorSetLayout samplerSetLayout;
VkDescriptorPool samplerDescriptorPool;

// All Textures + their Samplers
std::vector<Image> textureImages;
std::vector<VkDescriptorSet> samplerDescriptorSets;
std::vector<ETextureType> textureTypes;

// Materials
std::vector<Material> materials;

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
    vkDestroySampler(device, specularSampler, nullptr);
    vkDestroySampler(device, normalSampler, nullptr);
    
    vkDestroyDescriptorPool(device, samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, samplerSetLayout, nullptr);
}

VkDescriptorSetLayout MaterialManager::GetDescriptorSetLayout()
{
    return samplerSetLayout;
}

VkDescriptorSet MaterialManager::GetDescriptorSet(uint32_t materialId)
{
    if (materialId >= samplerDescriptorSets.size())
        throw std::runtime_error("Invalid Material ID: " + std::to_string(materialId));

    return samplerDescriptorSets[materialId];
}

ETextureType MaterialManager::GetTextureType(uint32_t textureId)
{
    if (textureId >= samplerDescriptorSets.size())
        throw std::runtime_error("Invalid Texture ID: " + std::to_string(textureId));
    
    return textureTypes[textureId];
}

Material MaterialManager::GetMaterial(uint32_t materialId)
{
    if (materialId >= materials.size())
        throw std::runtime_error("Invalid Material ID: " + std::to_string(materialId));

    return materials[materialId];
}

uint32_t createTexture(const std::string& fileName, ETextureType type, VkQueue queue, VkCommandPool commandPool, VkDescriptorSet descriptorSet)
{
    
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc* imageData;
    if (!fileName.empty())
        imageData = loadTextureImageFile(fileName, &width, &height, &imageSize);
    else
        imageData = loadTextureImageFile("plain.png", &width, &height, &imageSize);
    
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

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImages[id].GetImageView();

    if (type == ETextureType::DIFFUSE) imageInfo.sampler = textureSampler;
    if (type == ETextureType::SPECULAR) imageInfo.sampler = specularSampler;
    if (type == ETextureType::NORMAL) imageInfo.sampler = normalSampler;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;

    if (type == ETextureType::DIFFUSE) descriptorWrite.dstBinding = 0;
    if (type == ETextureType::SPECULAR) descriptorWrite.dstBinding = 1;
    if (type == ETextureType::NORMAL) descriptorWrite.dstBinding = 2;
    
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    return id;
}

uint32_t MaterialManager::CreateMaterial(const std::string& diffuse, const std::string& specular,
    const std::string& normal, VkQueue queue, VkCommandPool commandPool)
{
    VkDescriptorSet descriptorSet;

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = samplerDescriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    descriptorSetAllocInfo.pSetLayouts = &samplerSetLayout;

    VkResult result = vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet);
    CHECK_VK_RESULT(result, "Failed to allocate Descriptor Set for Image");
    
    Material mat = {};
    mat.diffuse = createTexture(diffuse, ETextureType::DIFFUSE, queue, commandPool, descriptorSet);
    mat.specular = createTexture(specular, ETextureType::SPECULAR, queue, commandPool, descriptorSet);
    mat.normal = createTexture(normal, ETextureType::NORMAL, queue, commandPool, descriptorSet);
    
    materials.push_back(mat);
    samplerDescriptorSets.push_back(descriptorSet);
    
    return static_cast<uint32_t>(samplerDescriptorSets.size())-1;
}

void createSetLayout()
{
    VkDescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding specularLayoutBinding;
    specularLayoutBinding.binding = 1;
    specularLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    specularLayoutBinding.descriptorCount = 1;
    specularLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    specularLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding normalLayoutBinding;
    normalLayoutBinding.binding = 2;
    normalLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalLayoutBinding.descriptorCount = 1;
    normalLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    normalLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings =
    {
        samplerLayoutBinding,
        specularLayoutBinding,
        normalLayoutBinding
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutCreateInfo.pBindings = bindings.data();

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

    result = vkCreateSampler(device, &samplerCreateInfo, nullptr, &normalSampler);
    CHECK_VK_RESULT(result, "Failed to create Normal Sampler");

    result = vkCreateSampler(device, &samplerCreateInfo, nullptr, &specularSampler);
    CHECK_VK_RESULT(result, "Failed to create Specular Sampler");
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
