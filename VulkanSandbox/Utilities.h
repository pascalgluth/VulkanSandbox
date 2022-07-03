#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <fstream>
#include <vector>
#include <array>

#define CHECK_VK_RESULT(result, str) if ((result) != VK_SUCCESS) throw std::runtime_error((str))

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,

#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

constexpr uint32_t MAX_TEXTURE_COUNT = 256;

struct UboFragSettings
{
    uint32_t bDrawShadowDepth;
};

struct UboDirLight
{
    glm::vec4 dlDirection = { 0.f, -1.f, -1.f, 0.f };

    glm::vec4 slPosition = { 0.f, 1.5f, 150.f, 0.f };
    glm::vec4 slDirection = { 0.f, 0.f, -1.f, 0.f };
    float slStrength = 10.f;
    float slCutoff = 60.f;
};

struct PushModel
{
    glm::mat4 model;
    uint32_t shaded;
};

struct Vertex
{
    glm::vec3 position;
    glm::vec2 textureCoord;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, textureCoord);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);
        
        return attributeDescriptions;
    }
};

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentationModes;

    bool isValid()
    {
        return !formats.empty() && !presentationModes.empty();
    }
};

struct QueueFamilyIndices
{
    int graphicsQueueFamily = -1;
    int presentationQueueFamily = -1;

    bool isValid()
    {
        return graphicsQueueFamily >= 0 && presentationQueueFamily >= 0;
    }
};

struct SwapChainImage
{
    VkImage image;
    VkImageView imageView;
};

static std::vector<char> readFile(const std::string& fileName)
{
    // Open File
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("Failed to read file: " + fileName);

    // Create vector with size
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> fileBuffer(fileSize);

    // Set read position to start of file
    file.seekg(0);

    // Read all file data into the buffer
    file.read(fileBuffer.data(), fileSize);

    // Close file
    file.close();
    
    return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    // Get properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((allowedTypes & (1 << i)) &&                                            // Index of memory tpe must match corresponding bit in allowedTypes
            (memoryProperties.memoryTypes[i].propertyFlags & properties))           // Desired property bit flags are port of memory property bit flags
                {
            // This memory type is valid, so return its index
            return i;
                }
    }

    return 0;
}

static VkFormat chooseSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& formats, VkImageTiling tiling,
    VkFormatFeatureFlags featureFlags)
{
    // Loop through options and find compatible one
    for (VkFormat format : formats)
    {
        // Get properties for given format on this device
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

        // Depending of tiling choice, need to check for different bit flag
        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a matching format");
}

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandPool = commandPool;
    commandBufferAllocInfo.commandBufferCount = 1;

    CHECK_VK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocInfo, &commandBuffer), "Failed to allocate Command Buffer");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    CHECK_VK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin Command Buffer");
    return commandBuffer;
}

static void endCommandBufferAndSubmit(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    CHECK_VK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit Command Buffer to Queue");

    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

static VkPipelineShaderStageCreateInfo loadShader(VkDevice device, const std::string& file, VkShaderStageFlagBits stage)
{
    auto shaderSpv = readFile("shaders/" + file);

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = shaderSpv.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderSpv.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);
    CHECK_VK_RESULT(result, "Failed to create Shader Module");

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = stage;
    shaderStageCreateInfo.module = shaderModule;
    shaderStageCreateInfo.pName = "main";

    return shaderStageCreateInfo;
}

static VkPipelineViewportStateCreateInfo defaultViewportFlipped(VkExtent2D extent)
{
    static VkViewport viewport = {};
    viewport.x = 0.f;
    viewport.y = static_cast<float>(extent.height);
    viewport.width = static_cast<float>(extent.width);
    viewport.height = -static_cast<float>(extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    static VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    return viewportStateCreateInfo;
}

static VkPipelineViewportStateCreateInfo defaultViewport(VkExtent2D extent)
{
    static VkViewport viewport = {};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    static VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    return viewportStateCreateInfo;
}

static VkPipelineRasterizationStateCreateInfo defaultRasterizerCreateInfo(VkCullModeFlags cullMode, VkFrontFace frontFace, VkBool32 depthBiasEnabled)
{
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.lineWidth = 1.f;
    rasterizerCreateInfo.cullMode = cullMode;
    rasterizerCreateInfo.frontFace = frontFace;
    rasterizerCreateInfo.depthBiasEnable = depthBiasEnabled;
    return rasterizerCreateInfo;
}