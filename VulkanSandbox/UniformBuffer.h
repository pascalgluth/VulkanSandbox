#pragma once

#include <vector>

#include "Buffer.h"
#include "Utilities.h"

template <typename T>
class UniformBuffer
{
public:
    UniformBuffer() {}
    ~UniformBuffer() {}

    void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkShaderStageFlags stage, VkDescriptorType type,
              uint32_t binding, uint32_t amount)
    {
        m_device = device;
        m_physicalDevice = physicalDevice;
        
        createLayout(type, stage, binding);
        createPool(type, amount);
        createBuffers(amount);
        createSets(type, amount);
    }
    
    void Destroy()
    {
        for (size_t i = 0; i < m_buffers.size(); ++i)
        {
            m_buffers[i].Destroy(m_device);
        }
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
    }

    void Update(uint32_t index)
    {
        void* data;
        vkMapMemory(m_device, m_buffers[index].GetMemory(), 0, sizeof(T), 0, &data);
        memcpy(data, &Data, sizeof(T));
        vkUnmapMemory(m_device, m_buffers[index].GetMemory());
    }

    VkDescriptorSetLayout GetLayout()
    {
        return m_setLayout;
    }

    VkDescriptorSet GetDescriptorSet(uint32_t index)
    {
        return m_descriptorSets[index];
    }

    T Data;

private:
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    
    VkDescriptorSetLayout m_setLayout;
    VkDescriptorPool m_descriptorPool;
    std::vector<Buffer> m_buffers;
    std::vector<VkDescriptorSet> m_descriptorSets;

    void createLayout(VkDescriptorType type, VkShaderStageFlags stage, uint32_t binding)
    {
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = type;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = stage;
        layoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = 1;
        layoutCreateInfo.pBindings = &layoutBinding;

        VkResult result = vkCreateDescriptorSetLayout(m_device, &layoutCreateInfo, nullptr, &m_setLayout);
        CHECK_VK_RESULT(result, "Failed to create Descriptor Set Layout");
    }

    void createPool(VkDescriptorType type, uint32_t count)
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = type;
        poolSize.descriptorCount = count;

        VkDescriptorPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.maxSets = count;
        poolCreateInfo.poolSizeCount = 1;
        poolCreateInfo.pPoolSizes = &poolSize;

        VkResult result = vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(result, "Failed to create Descriptor Pool");
    }

    void createBuffers(uint32_t count)
    {
        VkDeviceSize bufferSize = sizeof(T);
        
        m_buffers.resize(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            m_buffers[i].Init(m_device, m_physicalDevice,
                bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    void createSets(VkDescriptorType type, uint32_t count)
    {
        m_descriptorSets.resize(count);

        std::vector<VkDescriptorSetLayout> setLayouts(m_buffers.size(), m_setLayout);

        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = m_descriptorPool;
        descriptorSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(m_buffers.size());
        descriptorSetAllocInfo.pSetLayouts = setLayouts.data();

        VkResult result = vkAllocateDescriptorSets(m_device, &descriptorSetAllocInfo, m_descriptorSets.data());
        CHECK_VK_RESULT(result, "Failed to allocate descriptor sets");

        for (size_t i = 0; i < m_descriptorSets.size(); ++i)
        {
            VkDescriptorBufferInfo vpBufferInfo = {};
            vpBufferInfo.buffer = m_buffers[i].GetBuffer();
            vpBufferInfo.offset = 0;
            vpBufferInfo.range = sizeof(T);

            VkWriteDescriptorSet vpSetWrite = {};
            vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vpSetWrite.dstBinding = 0;
            vpSetWrite.dstSet = m_descriptorSets[i];
            vpSetWrite.dstArrayElement = 0;
            vpSetWrite.descriptorType = type;
            vpSetWrite.descriptorCount = 1;
            vpSetWrite.pBufferInfo = &vpBufferInfo;

            vkUpdateDescriptorSets(m_device, 1, &vpSetWrite, 0, nullptr);
        }
    }
};
