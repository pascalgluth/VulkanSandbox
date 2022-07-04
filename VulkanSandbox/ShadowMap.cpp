#include "ShadowMap.h"

#include "Object.h"

std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
VkDescriptorSetLayout setLayout;

std::vector<VkDescriptorPoolSize> poolSizes;
VkDescriptorPool descriptorPool;

std::vector<std::vector<VkDescriptorImageInfo*>> descriptorImageInfos;
std::vector<std::vector<VkWriteDescriptorSet>> descriptorWrites;
std::vector<VkDescriptorSet> descriptorSets;

ShadowMap::ShadowMap()
{
}

void ShadowMap::StaticInit(VkDevice device, uint32_t imageCount)
{
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutCreateInfo.pBindings = layoutBindings.data();

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &setLayout);
    CHECK_VK_RESULT(result, "Failed to create Descriptor Set Layout");

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    poolSizes.push_back(poolSize);

    VkDescriptorPoolSize poolSize2 = {};
    poolSize2.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize2.descriptorCount = 1;
    poolSizes.push_back(poolSize2);

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = imageCount;
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCreateInfo.pPoolSizes = poolSizes.data();

    result = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool);
    CHECK_VK_RESULT(result, "Failed to create Descriptor Pool");

    descriptorSets.resize(imageCount);
    std::vector<VkDescriptorSetLayout> layouts(imageCount, setLayout);
    
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = descriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = imageCount;
    descriptorSetAllocInfo.pSetLayouts = layouts.data();

    result = vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, descriptorSets.data());
    CHECK_VK_RESULT(result, "Failed to allocate Descriptor Set for Depth Image");
}

void ShadowMap::UpdateDescriptorSets(VkDevice device, uint32_t imageCount)
{
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites[i].size()), descriptorWrites[i].data(),
            0, nullptr);
    }
}

void ShadowMap::StaticDestroy(VkDevice device)
{
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

VkDescriptorSetLayout ShadowMap::GetDescriptorSetLayout()
{
    return setLayout;
}

VkDescriptorSet ShadowMap::GetDescriptorSet(uint32_t imageIndex)
{
    return descriptorSets[imageIndex];
}

void ShadowMap::Init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t imageCount, float shadowMapWidth, float shadowMapHeight, uint32_t
                     binding)
{
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_imageCount = imageCount;
    m_shadowExtent.width = static_cast<uint32_t>(shadowMapWidth);
    m_shadowExtent.height = static_cast<uint32_t>(shadowMapHeight);

    if (descriptorWrites.size() != imageCount)
    {
        descriptorImageInfos.resize(imageCount);
        descriptorWrites.resize(imageCount);
        descriptorSets.resize(imageCount);
    }

    m_uboLightPerspective.Init(device, physicalDevice, VK_SHADER_STAGE_VERTEX_BIT,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, imageCount);

    createDescriptorBinding(binding);
    createShadowMapImageAndSampler();
}

void ShadowMap::FinishInit(uint32_t binding)
{
    createDescriptorWrites(binding);
    createShadowMapRenderPass();
    createShadowMapFrameBuffers();
    createPipeline();
}

void ShadowMap::Destroy()
{
    vkDestroyPipeline(m_device, m_shadowMapPassPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowMapPassPipelineLayout, nullptr);

    vkDestroyRenderPass(m_device, m_shadowMapRenderPass, nullptr);
    
    for (size_t i = 0; i < m_shadowMapImage.size(); ++i)
    {
        vkDestroyFramebuffer(m_device, m_shadowMapFramebuffers[i], nullptr);
        m_shadowMapImage[i].Destroy(m_device);
    }

    vkDestroySampler(m_device, m_shadowMapSampler, nullptr);

    m_uboLightPerspective.Destroy();
}

UboViewProjection* ShadowMap::PerspectiveData()
{
    return &m_uboLightPerspective.Data;
}

void ShadowMap::UpdateUbo(uint32_t imageIndex)
{
    m_uboLightPerspective.Update(imageIndex);
}

void ShadowMap::RecordCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<Object*>& objects)
{
    std::array<VkClearValue, 1> clearValues = {};
    clearValues[0].depthStencil.depth = 1.f;
    clearValues[0].depthStencil.stencil = 0;
    
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_shadowMapRenderPass;
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = m_shadowExtent;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();
    renderPassBeginInfo.framebuffer = m_shadowMapFramebuffers[imageIndex];
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowMapPassPipeline);

        for (size_t j = 0; j < objects.size(); ++j)
        {
            if (objects[j]->Name == "Light")
                continue;
            
            const glm::mat4& objectTransform = objects[j]->GetTransform();
            std::vector<Mesh>& meshes = objects[j]->GetMeshes();

            for (size_t i = 0; i < meshes.size(); ++i)
            {
                std::vector<VkDescriptorSet> bindDescriptorSets =
                {
                    m_uboLightPerspective.GetDescriptorSet(imageIndex),
                };
            
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowMapPassPipelineLayout,
                    0, static_cast<uint32_t>(bindDescriptorSets.size()), bindDescriptorSets.data(),
                    0, nullptr);
            
                VkBuffer vertexBuffers[] = { meshes[i].GetVertexBuffer()->GetBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

                PushModel pushModel = {};
                pushModel.model = objectTransform * meshes[i].GetTransform();
                vkCmdPushConstants(commandBuffer, m_shadowMapPassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushModel), &pushModel);
            
                if (meshes[i].Indexed())
                {
                    vkCmdBindIndexBuffer(commandBuffer, meshes[i].GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commandBuffer, meshes[i].GetIndexCount(), 1, 0, 0, 0);
                }
                else
                {
                    vkCmdDraw(commandBuffer, meshes[i].GetVertexCount(), 1, 0, 0);
                }
            }
        }
    }
    vkCmdEndRenderPass(commandBuffer);
}

VkImageView ShadowMap::GetAnImageView()
{
    return m_shadowMapImage[0].GetImageView();
}

VkSampler ShadowMap::GetSampler()
{
    return m_shadowMapSampler;
}

void ShadowMap::createDescriptorBinding(uint32_t binding)
{
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding.pImmutableSamplers = nullptr;

    layoutBindings.push_back(layoutBinding);
}

void ShadowMap::createDescriptorWrites(uint32_t binding)
{
    for (size_t i = 0; i < m_shadowMapImage.size(); ++i)
    {
        auto imageInfo = new VkDescriptorImageInfo();
        imageInfo->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfo->imageView = m_shadowMapImage[i].GetImageView();
        imageInfo->sampler = m_shadowMapSampler;

        descriptorImageInfos[i].push_back(imageInfo);
        
        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = imageInfo;

        descriptorWrites[i].push_back(descriptorWrite);
    }
}

void ShadowMap::createShadowMapImageAndSampler()
{
    m_shadowMapImage.resize(m_imageCount);
    
    for (size_t i = 0; i < m_shadowMapImage.size(); ++i)
    {
        m_shadowMapImage[i].Init(m_device, m_physicalDevice,
            m_shadowExtent.width, m_shadowExtent.height, VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.f;
    samplerCreateInfo.minLod = 0.f;
    samplerCreateInfo.maxLod = 0.f;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;

    VkResult result = vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_shadowMapSampler);
    CHECK_VK_RESULT(result, "Failed to create Depth Image Sampler");
}

void ShadowMap::createShadowMapRenderPass()
{
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef;
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstSubpass = 0;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    //dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &depthAttachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();

    VkResult result = vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_shadowMapRenderPass);
    CHECK_VK_RESULT(result, "Failed to create Render Pass");
}

void ShadowMap::createShadowMapFrameBuffers()
{
    m_shadowMapFramebuffers.resize(m_imageCount);

    for (size_t i = 0; i < m_shadowMapFramebuffers.size(); ++i)
    {
        std::array<VkImageView, 1> attachments =
        {
            m_shadowMapImage[i].GetImageView()
        };

        VkFramebufferCreateInfo frameBufferCreateInfo = {};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = m_shadowMapRenderPass;
        frameBufferCreateInfo.width = m_shadowExtent.width;
        frameBufferCreateInfo.height = m_shadowExtent.height;
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferCreateInfo.pAttachments = attachments.data();
        frameBufferCreateInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_shadowMapFramebuffers[i]);
        CHECK_VK_RESULT(result, "Failed to create Framebuffer");
    }
}

void ShadowMap::createPipeline()
{
    VkPipelineShaderStageCreateInfo shaderStages[] = { loadShader(m_device, "depthMap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT) };
    
    auto vertexBindingDescription = Vertex::getBindingDescription();
    auto vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

    // -- INPUT ASSEMBLER --

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStageCreateInfo = {};
    inputAssemblyStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStageCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStageCreateInfo.primitiveRestartEnable = VK_FALSE;

    // -- VIEWPORT & SCISSOR --

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = defaultViewport(m_shadowExtent);

    // -- RASTERIZER --

    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo =
        defaultRasterizerCreateInfo(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE);
    
    // -- MULTISAMPLING --

    VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
    multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // -- BLENDING --

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
    colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendCreateInfo.attachmentCount = 1;
    colorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;

    // -- PIPELINE LAYOUT --

    VkPushConstantRange worldPushConstantRange = {};
    worldPushConstantRange.size = sizeof(PushModel);
    worldPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    worldPushConstantRange.offset = 0;
    
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &worldPushConstantRange;

    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        m_uboLightPerspective.GetLayout()
    };

    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();

    VkResult result = vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_shadowMapPassPipelineLayout);
    CHECK_VK_RESULT(result, "Failed to create Pipeline Layout");

    // -- DEPTH STENCIL TESTING --

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

    // -- PIPELINE CREATION --

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 1;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStageCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.layout = m_shadowMapPassPipelineLayout;
    pipelineCreateInfo.renderPass = m_shadowMapRenderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_shadowMapPassPipeline);
    CHECK_VK_RESULT(result, "Failed to create Graphics Pipeline");
}
