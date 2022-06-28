#include "VulkanRenderer.h"

#include <SDL_vulkan.h>
#include <iostream>
#include <set>
#include <stdexcept>

#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_vulkan.h"

#include "Engine.h"
#include "MaterialManager.h"
#include "Window.h"

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::Init()
{
    try
    {
        createInstance();
        createWindowSurface();
        getPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createDepthBufferImage();
        createRenderPass();
        
        m_uboViewProjection.Init(m_device.logicalDevice, m_device.physicalDevice,
            VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
            static_cast<uint32_t>(m_swapchainImages.size()));

        MaterialManager::Init(m_device.logicalDevice, m_device.physicalDevice);
        
        createPipeline();
        createFrameBuffers();
        createCommandPool();
        createGraphicsCommandBuffer();
        createSynchronization();

        initImGui();

        m_camera.SetProjection(90.f, static_cast<float>(m_swapchainExtent.width)/static_cast<float>(m_swapchainExtent.height), 0.01f, 10000.f);
        m_camera.AddPositionOffset(0.f, 0.f, 1.f);

        m_object.Init(m_device.logicalDevice, m_device.physicalDevice, m_graphicsQueue, m_graphicsCommandPool, "objects/SmallBuilding01.obj");
    }
    catch (const std::runtime_error& err)
    {
        std::cout << "Error: " << err.what() << std::endl;
        exit(-1);
    }
}

void VulkanRenderer::Destroy()
{
    vkDeviceWaitIdle(m_device.logicalDevice);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_device.logicalDevice, m_imguiDescriptorPool, nullptr);

    //m_testMesh.Destroy();
    m_object.Destroy();
    
    for (size_t i = 0; i < m_imageAvailable.size(); ++i)
    {
        vkDestroySemaphore(m_device.logicalDevice, m_imageAvailable[i], nullptr);
        vkDestroySemaphore(m_device.logicalDevice, m_renderFinished[i], nullptr);
        vkDestroyFence(m_device.logicalDevice, m_waitForDrawFinished[i], nullptr);
    }
    
    vkFreeCommandBuffers(m_device.logicalDevice, m_graphicsCommandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
    vkDestroyCommandPool(m_device.logicalDevice, m_graphicsCommandPool, nullptr);
    
    vkDestroyRenderPass(m_device.logicalDevice, m_renderPass, nullptr);

    for (size_t i = 0; i < m_colorResolveImage.size(); ++i)
    {
        m_colorResolveImage[i].Destroy(m_device.logicalDevice);
    }
    
    for (size_t i = 0; i < m_depthBufferImage.size(); ++i)
    {
        m_depthBufferImage[i].Destroy(m_device.logicalDevice);
    }

    for (size_t i = 0; i < m_swapchainFrameBuffers.size(); ++i)
    {
        vkDestroyFramebuffer(m_device.logicalDevice, m_swapchainFrameBuffers[i], nullptr);
    }
    
    vkDestroyPipeline(m_device.logicalDevice, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device.logicalDevice, m_graphicsPipelineLayout, nullptr);

    m_uboViewProjection.Destroy();
    MaterialManager::Destroy();
    
    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        vkDestroyImageView(m_device.logicalDevice, m_swapchainImages[i].imageView, nullptr);
    }
    
    vkDestroySwapchainKHR(m_device.logicalDevice, m_swapchain, nullptr);
    vkDestroyDevice(m_device.logicalDevice, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::Draw()
{
    static int currentFrame = 0;

    vkWaitForFences(m_device.logicalDevice, 1, &m_waitForDrawFinished[currentFrame], VK_TRUE, 0x7FFFFFFF);
    vkResetFences(m_device.logicalDevice, 1, &m_waitForDrawFinished[currentFrame]);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(Engine::GetWindow()->GetSDLWindow());
    ImGui::NewFrame();
    renderImGui();
    ImGui::Render();
    
    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device.logicalDevice, m_swapchain, 0x7FFFFFFF, m_imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    m_uboViewProjection.Data.view = m_camera.GetViewMatrix();
    m_uboViewProjection.Data.projection = m_camera.GetProjectionMatrix();
    m_uboViewProjection.Update(imageIndex);
    recordCommands(imageIndex);

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailable[currentFrame];
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinished[currentFrame];

    VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_waitForDrawFinished[currentFrame]);
    CHECK_VK_RESULT(result, "Failed to submit Command Buffer to Queue");

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinished[currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
    CHECK_VK_RESULT(result, "Failed to present Image");

    currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
}

void VulkanRenderer::renderImGui()
{
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::EndMainMenuBar();
    }

    ImGui::Begin("Camera");
    {
        glm::vec3 camPos = m_camera.GetPosition();
        if (ImGui::DragFloat3("Position", &camPos.x, 0.01f))
            m_camera.SetPosition(camPos);

        glm::vec3 camRot = m_camera.GetRotation();
        if (ImGui::DragFloat3("Rotation", &camRot.x, 1.f))
            m_camera.SetRotation(camRot);
    }
    ImGui::End();

    ImGui::Begin("Building");
    {
        glm::vec3 pos = m_object.GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
            m_object.SetPosition(pos);
    }
    ImGui::End();
}

void VulkanRenderer::createInstance()
{
    SDL_Window* window = Engine::GetWindow()->GetSDLWindow();

    if (this->m_enableValidationLayers && !checkValidationLayerSupport()) 
        throw std::runtime_error("Validation layers requested, but not available!");
    
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Sandbox";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Sandbox";
    appInfo.engineVersion = VK_MAKE_VERSION(2, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> instanceExtensions;

    uint32_t sdlExtensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr);
    instanceExtensions.resize(sdlExtensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, instanceExtensions.data());

    if (!checkInstanceExtensionSupport(instanceExtensions))
        throw std::runtime_error("Instance does not support required extensions by SDL");

    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if (m_enableValidationLayers)
    {
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = m_validationLayers.data();
    }
    else
    {
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.ppEnabledLayerNames = nullptr;
    }
    
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    CHECK_VK_RESULT(result, "Failed to create Vulkan Instance");
}

void VulkanRenderer::createWindowSurface()
{
    SDL_Window* window = Engine::GetWindow()->GetSDLWindow();
    if (!SDL_Vulkan_CreateSurface(window, m_instance, &m_surface))
        throw std::runtime_error("Failed to create Window Surface");
}

void VulkanRenderer::createLogicalDevice()
{
    QueueFamilyIndices indices = getQueueFamilies(m_device.physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndices = { indices.graphicsQueueFamily, indices.presentationQueueFamily };

    for (int queueFamilyIndex : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        float priority = 1.f;
        queueCreateInfo.pQueuePriorities = &priority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VkResult result = vkCreateDevice(m_device.physicalDevice, &deviceCreateInfo, nullptr, &m_device.logicalDevice);
    CHECK_VK_RESULT(result, "Failed to create Logical Device");

    vkGetDeviceQueue(m_device.logicalDevice, indices.graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device.logicalDevice, indices.presentationQueueFamily, 0, &m_presentationQueue);
}

void VulkanRenderer::createSwapchain()
{
    SwapChainDetails swapchainDetails = getSwapchainDetails(m_device.physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(swapchainDetails.formats);
    VkPresentModeKHR presentMode = chooseSwapchainPresentMode(swapchainDetails.presentationModes);
    VkExtent2D extent = chooseSwapchainExtent(swapchainDetails.surfaceCapabilities);
    std::cout << "Using Resolution: " << extent.width << "x" << extent.height << std::endl;

    uint32_t imageCount = swapchainDetails.surfaceCapabilities.minImageCount + 1;
    if (swapchainDetails.surfaceCapabilities.maxImageCount > 0 &&
            swapchainDetails.surfaceCapabilities.maxImageCount < imageCount)
    {
        imageCount = swapchainDetails.surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = m_surface;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = swapchainDetails.surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;

    QueueFamilyIndices indices = getQueueFamilies(m_device.physicalDevice);

    if (indices.graphicsQueueFamily != indices.presentationQueueFamily)
    {
        uint32_t queueFamilyIndices[] = { static_cast<uint32_t>(indices.graphicsQueueFamily), static_cast<uint32_t>(indices.presentationQueueFamily) };
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_device.logicalDevice, &swapchainCreateInfo, nullptr, &m_swapchain);
    CHECK_VK_RESULT(result, "Failed to create Swapchain");

    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapchain, &swapchainImageCount, nullptr);
    auto images = std::vector<VkImage>(swapchainImageCount);
    vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapchain, &swapchainImageCount, images.data());

    for (VkImage image : images)
    {
        SwapChainImage scImage = {};
        scImage.image = image;
        scImage.imageView = Image::CreateImageView(m_device.logicalDevice, image, m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        m_swapchainImages.push_back(scImage);
    }

    m_colorResolveImage.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        m_colorResolveImage[i].Init(m_device.logicalDevice, m_device.physicalDevice,
            m_swapchainExtent.width, m_swapchainExtent.height, m_swapchainImageFormat,
            m_msaaSamples, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanRenderer::createPipeline()
{
    // -- SHADERS --
    
    auto vertexShaderSpv = readFile("shaders/vert.spv");
    auto fragmentShaderSpv = readFile("shaders/frag.spv");

    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderSpv);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderSpv);

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShaderModule;
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShaderModule;
    fragmentShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

    // -- VERTEX INPUT --

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

    VkViewport viewport = {};
    viewport.x = 0.f;
    viewport.y = static_cast<float>(m_swapchainExtent.height);
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = -static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    // -- RASTERIZER --

    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.lineWidth = 1.f;
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

    // -- MULTISAMPLING --

    VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
    multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleCreateInfo.rasterizationSamples = m_msaaSamples;

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
    worldPushConstantRange.size = sizeof(glm::mat4);
    worldPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    worldPushConstantRange.offset = 0;
    
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &worldPushConstantRange;

    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        m_uboViewProjection.GetLayout(),
        MaterialManager::GetDescriptorSetLayout()
    };

    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();

    VkResult result = vkCreatePipelineLayout(m_device.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_graphicsPipelineLayout);
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
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStageCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.layout = m_graphicsPipelineLayout;
    pipelineCreateInfo.renderPass = m_renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(m_device.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphicsPipeline);
    CHECK_VK_RESULT(result, "Failed to create Graphics Pipeline");

    vkDestroyShaderModule(m_device.logicalDevice, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_device.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::createDepthBufferImage()
{
    m_depthBufferImage.resize(m_swapchainImages.size());

    m_depthBufferImageFormat = chooseSupportedFormat(m_device.physicalDevice,
        {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    for (size_t i = 0; i < m_depthBufferImage.size(); ++i)
    {
        m_depthBufferImage[i].Init(m_device.logicalDevice, m_device.physicalDevice,
            m_swapchainExtent.width, m_swapchainExtent.height, m_depthBufferImageFormat,
            m_msaaSamples, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    
}

void VulkanRenderer::createFrameBuffers()
{
    m_swapchainFrameBuffers.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainFrameBuffers.size(); ++i)
    {
        std::array<VkImageView, 3> attachments =
        {
            m_colorResolveImage[i].GetImageView(),
            m_depthBufferImage[i].GetImageView(),
            m_swapchainImages[i].imageView
        };

        VkFramebufferCreateInfo frameBufferCreateInfo = {};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = m_renderPass;
        frameBufferCreateInfo.width = m_swapchainExtent.width;
        frameBufferCreateInfo.height = m_swapchainExtent.height;
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferCreateInfo.pAttachments = attachments.data();
        frameBufferCreateInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(m_device.logicalDevice, &frameBufferCreateInfo, nullptr, &m_swapchainFrameBuffers[i]);
        CHECK_VK_RESULT(result, "Failed to create Framebuffer");
    }
}

void VulkanRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_swapchainImageFormat;
    colorAttachment.samples = m_msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorResolveAttachment = {};
    colorResolveAttachment.format = m_swapchainImageFormat;
    colorResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = m_depthBufferImageFormat;
    depthAttachment.samples = m_msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorResolveAttachmentRef = {};
    colorResolveAttachmentRef.attachment = 2;
    colorResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorResolveAttachmentRef;

    std::array<VkSubpassDependency, 2> subpassDependencies{};

    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstSubpass = 0;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorResolveAttachment };

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassCreateInfo.pAttachments = attachments.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(m_device.logicalDevice, &renderPassCreateInfo, nullptr, &m_renderPass);
    CHECK_VK_RESULT(result, "Failed to create Render Pass");
}

void VulkanRenderer::createCommandPool()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = getQueueFamilies(m_device.physicalDevice).graphicsQueueFamily;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(m_device.logicalDevice, &commandPoolCreateInfo, nullptr, &m_graphicsCommandPool);
    CHECK_VK_RESULT(result, "Failed to create Command Pool");
}

void VulkanRenderer::createGraphicsCommandBuffer()
{
    m_commandBuffers.resize(m_swapchainImages.size());

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = m_graphicsCommandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(m_device.logicalDevice, &commandBufferAllocInfo, m_commandBuffers.data());
    CHECK_VK_RESULT(result, "Failed to allocate Command Buffers");
}

void VulkanRenderer::initImGui()
{
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = std::size(poolSizes);
    poolCreateInfo.pPoolSizes = poolSizes;

    CHECK_VK_RESULT(vkCreateDescriptorPool(m_device.logicalDevice, &poolCreateInfo, nullptr, &m_imguiDescriptorPool), "Failed to create ImGui Descriptor Pool");

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(Engine::GetWindow()->GetSDLWindow());

    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = m_instance;
    imguiInitInfo.PhysicalDevice = m_device.physicalDevice;
    imguiInitInfo.Device = m_device.logicalDevice;
    imguiInitInfo.Queue = m_graphicsQueue;
    imguiInitInfo.DescriptorPool = m_imguiDescriptorPool;
    imguiInitInfo.MinImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    imguiInitInfo.ImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    imguiInitInfo.MSAASamples = m_msaaSamples;

    ImGui_ImplVulkan_Init(&imguiInitInfo, m_renderPass);

    VkCommandBuffer commandBuffer = beginCommandBuffer(m_device.logicalDevice, m_graphicsCommandPool);
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    endCommandBufferAndSubmit(m_device.logicalDevice, m_graphicsCommandPool, m_graphicsQueue, commandBuffer);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = { 0.f, 0.f, 0.f, 1.f };
    clearValues[1].depthStencil.depth = 1.f;
    clearValues[1].depthStencil.stencil = 0.f;

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_renderPass;
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = m_swapchainExtent;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();
    renderPassBeginInfo.framebuffer = m_swapchainFrameBuffers[currentImage];
    
    if (vkBeginCommandBuffer(m_commandBuffers[currentImage], &commandBufferBeginInfo) == VK_SUCCESS)
    {
        vkCmdBeginRenderPass(m_commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            vkCmdBindPipeline(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            

            const glm::mat4& objectTransform = m_object.GetTransform();

            std::vector<Mesh>& meshes = m_object.GetMeshes();

            for (size_t i = 0; i < meshes.size(); ++i)
            {
                std::vector<VkDescriptorSet> descriptorSets =
                {
                    m_uboViewProjection.GetDescriptorSet(currentImage),
                    MaterialManager::GetDescriptorSet(meshes[i].GetMaterialId())
                };
                
                vkCmdBindDescriptorSets(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout,
                    0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
                    0, nullptr);
                
                VkBuffer vertexBuffers[] = { meshes[i].GetVertexBuffer()->GetBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(m_commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);

                glm::mat4 transform = objectTransform * meshes[i].GetTransform();
                vkCmdPushConstants(m_commandBuffers[currentImage], m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
                
                if (meshes[i].Indexed())
                {
                    vkCmdBindIndexBuffer(m_commandBuffers[currentImage], meshes[i].GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(m_commandBuffers[currentImage], meshes[i].GetIndexCount(), 1, 0, 0, 0);
                }
                else
                {
                    vkCmdDraw(m_commandBuffers[currentImage], meshes[i].GetVertexCount(), 1, 0, 0);
                }
            }

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[currentImage]);
        }
        vkCmdEndRenderPass(m_commandBuffers[currentImage]);
        
        vkEndCommandBuffer(m_commandBuffers[currentImage]);
    }
    else
    {
        throw std::runtime_error("Failed to begin Command Buffer");
    }
}

void VulkanRenderer::getPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) throw std::runtime_error("No device found");

    auto devices = std::vector<VkPhysicalDevice>(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        if (checkDeviceSuitable(device))
        {
            m_device.physicalDevice = device;

            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            std::cout << "Using Device: " << deviceProperties.deviceName << std::endl;

            m_msaaSamples = getMaxUsableSampleCount(device);

            return;
        }
    }

    throw std::runtime_error("Failed to find Device");
}

SwapChainDetails VulkanRenderer::getSwapchainDetails(VkPhysicalDevice device)
{
    SwapChainDetails swapChainDetails;

    // Get Capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapChainDetails.surfaceCapabilities);

    // Get Formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

    if (formatCount > 0)
    {
        swapChainDetails.formats.resize(formatCount); 
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, swapChainDetails.formats.data());
    }

    // Get Presentation Modes
    uint32_t presentationModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationModeCount, nullptr);

    if (presentationModeCount > 0)
    {
        swapChainDetails.presentationModes.resize(presentationModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationModeCount, swapChainDetails.presentationModes.data());
    }
    
    return swapChainDetails;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& spv)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = spv.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(m_device.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
    CHECK_VK_RESULT(result, "Failed to create Shader Module");
    return shaderModule;
}

void VulkanRenderer::createSynchronization()
{
    m_imageAvailable.resize(MAX_CONCURRENT_FRAMES);
    m_renderFinished.resize(MAX_CONCURRENT_FRAMES);
    m_waitForDrawFinished.resize(MAX_CONCURRENT_FRAMES);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        VkResult result = vkCreateSemaphore(m_device.logicalDevice, &semaphoreCreateInfo, nullptr, &m_imageAvailable[i]);
        CHECK_VK_RESULT(result, "Failed to create Semaphore");

        result = vkCreateSemaphore(m_device.logicalDevice, &semaphoreCreateInfo, nullptr, &m_renderFinished[i]);
        CHECK_VK_RESULT(result, "Failed to create Semaphore");

        result = vkCreateFence(m_device.logicalDevice, &fenceCreateInfo, nullptr, &m_waitForDrawFinished[i]);
        CHECK_VK_RESULT(result, "Failed to create Fence");
    }
}

bool VulkanRenderer::checkInstanceExtensionSupport(const std::vector<const char*>& extensions)
{
    uint32_t vulkanExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &vulkanExtensionCount, nullptr);

    auto vulkanExtensions = std::vector<VkExtensionProperties>(vulkanExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &vulkanExtensionCount, vulkanExtensions.data());

    for (const auto& extension : extensions)
    {
        bool hasExtension = false;
        for (const auto& vulkanExtension : vulkanExtensions)
        {
            if (strcmp(extension, vulkanExtension.extensionName) == 0)
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension) return false;
    }

    return true;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    auto queueFamilyList = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilyList)
    {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsQueueFamily = i; // If queue family is valid, then get index
        }

        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentationSupport);
        if (queueFamily.queueCount > 0 && presentationSupport)
        {
            indices.presentationQueueFamily = i;
        }
        
        if (indices.isValid()) break;
        i++;
    }

    return indices;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indices = getQueueFamilies(device);
    if (!indices.isValid()) return false;
    
    if (!checkDeviceExtensionSupport(device)) return false;
    
    SwapChainDetails swapChainDetails = getSwapchainDetails(device);
    if (!swapChainDetails.isValid()) return false;

    return deviceFeatures.samplerAnisotropy;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    if (extensionCount == 0) return false;
    
    auto extensions = std::vector<VkExtensionProperties>(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    for (const auto& deviceExtension : deviceExtensions)
    {
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if (strcmp(deviceExtension, extension.extensionName) == 0)
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension) return false;
    }

    return true;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    for (const auto& format : formats)
    {
        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& modes)
{
    for (const auto& mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return surfaceCapabilities.currentExtent;
    }

    SDL_Window* window = Engine::GetWindow()->GetSDLWindow();

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    VkExtent2D newExtent = {};
    newExtent.width = static_cast<uint32_t>(width);
    newExtent.height = static_cast<uint32_t>(height);

    // Surface also defines max and min extents, so make sure within boundaries by clamping value
    newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
    newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));
    
    return newExtent;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
 
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		
    for (const char* layerName : this->m_validationLayers) {
        bool layerFound = false;
 
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
 
        if (!layerFound) {
            return false;
        }
    }
 
    return true;
}
