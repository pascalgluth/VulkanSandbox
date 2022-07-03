#pragma once

#include "Camera.h"
#include "Image.h"
#include "UniformBuffer.h"

class Object;

class ShadowMap
{
public:
    ShadowMap();

    static void StaticInit(VkDevice device, uint32_t imageCount);
    static void UpdateDescriptorSets(VkDevice device, uint32_t imageCount);
    static void StaticDestroy(VkDevice device);

    static VkDescriptorSetLayout GetDescriptorSetLayout();
    static VkDescriptorSet GetDescriptorSet(uint32_t imageIndex);
    
    void Init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t imageCount, float shadowMapWidth, float shadowMapHeight, uint32_t
              binding);
    void FinishInit(uint32_t binding);
    void Destroy();

    UboViewProjection* PerspectiveData();
    void UpdateUbo(uint32_t imageIndex);

    void RecordCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<Object*>& objects);

    VkImageView GetAnImageView();
    VkSampler GetSampler();
    
private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    uint32_t m_imageCount;
    
    UniformBuffer<UboViewProjection> m_uboLightPerspective;
    VkExtent2D m_shadowExtent;
    std::vector<Image> m_shadowMapImage;
    VkPipeline m_shadowMapPassPipeline;
    VkPipelineLayout m_shadowMapPassPipelineLayout;
    VkRenderPass m_shadowMapRenderPass;
    std::vector<VkFramebuffer> m_shadowMapFramebuffers;
    VkSampler m_shadowMapSampler;

    void createDescriptorBinding(uint32_t binding);
    void createShadowMapImageAndSampler();
    void createDescriptorWrites(uint32_t binding);
    void createShadowMapRenderPass();
    void createShadowMapFrameBuffers();
    void createPipeline();
    
};
