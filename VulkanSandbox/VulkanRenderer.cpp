#include "VulkanRenderer.h"

#include <SDL_vulkan.h>
#include <iostream>
#include <set>
#include <stdexcept>

#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/ImGuizmo.h"

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

        m_uboPointLight.Init(m_device.logicalDevice, m_device.physicalDevice,
            VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
            static_cast<uint32_t>(m_swapchainImages.size()));

        m_uboFragSettings.Init(m_device.logicalDevice, m_device.physicalDevice,
            VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
            static_cast<uint32_t>(m_swapchainImages.size()));

        MaterialManager::Init(m_device.logicalDevice, m_device.physicalDevice);

        m_dlShadowMap.Init(m_device.logicalDevice, m_device.physicalDevice,
            static_cast<uint32_t>(m_swapchainImages.size()), 1024, 1024,
            0);

        m_slShadowMap.Init(m_device.logicalDevice, m_device.physicalDevice,
            static_cast<uint32_t>(m_swapchainImages.size()), 1024, 1024,
            1);

        ShadowMap::StaticInit(m_device.logicalDevice, static_cast<uint32_t>(m_swapchainImages.size()));

        m_dlShadowMap.FinishInit(0);
        m_slShadowMap.FinishInit(1);

        ShadowMap::UpdateDescriptorSets(m_device.logicalDevice, static_cast<uint32_t>(m_swapchainImages.size()));
        
        createPipeline();
        createFrameBuffers();
        createCommandPool();
        createGraphicsCommandBuffer();
        createSynchronization();

        initImGui();

        m_camera.SetProjection(90.f, static_cast<float>(m_swapchainExtent.width)/static_cast<float>(m_swapchainExtent.height), 0.01f, 100000.f);
        m_camera.AddPositionOffset(-25.2f, 23.36f, 29.65f);
        m_camera.AddRotation(-13.f, -65.f, 0.f);
        
        auto obj = new Object("Building");
        m_objects.push_back(obj);
        obj->Init(m_device.logicalDevice, m_device.physicalDevice, m_graphicsQueue, m_graphicsCommandPool, "objects/SmallBuilding01.obj");
        obj->SetPosition({0.f, -8.7f, 0.f});
        obj->SetScale({10.f, 10.f, 10.f});

        auto obj2 = new Object("Ground");
        m_objects.push_back(obj2);
        obj2->Init(m_device.logicalDevice, m_device.physicalDevice, m_graphicsQueue, m_graphicsCommandPool, "objects/Untitled-1.obj");
        obj2->SetPosition({0.f, -25.f, 0.f});
        obj2->SetScale({100.f, 0.5f, 800.f});

        auto light = new Object("Light");
        m_objects.push_back(light);
        light->Init(m_device.logicalDevice, m_device.physicalDevice, m_graphicsQueue, m_graphicsCommandPool, "objects/light.obj");
        //light->SetPosition(glm::vec3(0.f, 5.f, 10.f));
        light->SetScale(glm::vec3(0.5f, 0.5f, 0.5f));
    }
    catch (const std::runtime_error& err)
    {
        std::cout << "Error: " << err.what() << std::endl;
        exit(-1);
    }
}

void VulkanRenderer::Update(float deltaTime)
{
    float cameraSpeed = 50.f * deltaTime;
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
        cameraSpeed *= 2.f;

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::GetMouseCursor() != ImGuiMouseCursor_None)
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::GetMouseCursor() != ImGuiMouseCursor_Arrow)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

    if (ImGui::IsKeyDown(ImGuiKey_W))
        m_camera.AddPositionOffset(glm::normalize(m_camera.GetForwardVector()) * cameraSpeed);
    if (ImGui::IsKeyDown(ImGuiKey_S))
        m_camera.AddPositionOffset(-glm::normalize(m_camera.GetForwardVector()) * cameraSpeed);
    if (ImGui::IsKeyDown(ImGuiKey_A))
        m_camera.AddPositionOffset(-glm::normalize(glm::cross(m_camera.GetForwardVector(), m_camera.GetUpVector())) * cameraSpeed);
    if (ImGui::IsKeyDown(ImGuiKey_D))
        m_camera.AddPositionOffset(glm::normalize(glm::cross(m_camera.GetForwardVector(), m_camera.GetUpVector())) * cameraSpeed);

    static ImVec2 firstPos = ImGui::GetMousePos();

    ImVec2 currentPos = ImGui::GetMousePos();
    if (currentPos.x != firstPos.x || currentPos.y != firstPos.y)
    {
        float offsetX = firstPos.x - currentPos.x;
        float offsetY = firstPos.y - currentPos.y;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            m_camera.AddRotation(0.f, offsetY, -offsetX);   
        firstPos = currentPos;
    }

    glm::vec3 vec = { 0.f, 30.f, 0.f };
    vec = glm::rotate(vec, glm::radians(m_rad), glm::vec3(1.f, 0.f, 0.f));
    m_objects[2]->SetPosition(vec);
}

void VulkanRenderer::Destroy()
{
    vkDeviceWaitIdle(m_device.logicalDevice);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_device.logicalDevice, m_imguiDescriptorPool, nullptr);

    m_uboPointLight.Destroy();
    m_uboViewProjection.Destroy();
    MaterialManager::Destroy();
    m_uboFragSettings.Destroy();

    m_dlShadowMap.Destroy();
    m_slShadowMap.Destroy();
    ShadowMap::StaticDestroy(m_device.logicalDevice);

    //m_testMesh.Destroy();
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        m_objects[i]->Destroy();
        delete m_objects[i];
    }
    
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

    //m_uboLightPerspective.Data.view = glm::lookAt(glm::vec3(0.f, 4.f, 0.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));

    m_dlShadowMap.PerspectiveData()->view = glm::mat4(glm::angleAxis(glm::radians(m_rad), glm::vec3(1.f, 0.f, 0.f)));
    m_dlShadowMap.PerspectiveData()->projection = glm::orthoZO(m_leftRight.x, m_leftRight.y, m_topBottom.x, m_topBottom.y, m_nearFar.x, m_nearFar.y);
    m_dlShadowMap.UpdateUbo(imageIndex);

    glm::vec3 position = m_uboPointLight.Data.slPosition;
    glm::vec3 direction = glm::vec3(m_uboPointLight.Data.slDirection) + position;

    glm::mat4 view = glm::lookAt(position, direction, glm::vec3(0.f, 1.f, 0.f));
    
    m_slShadowMap.PerspectiveData()->view = view;
    m_slShadowMap.PerspectiveData()->projection = glm::perspectiveZO(90.f, 1.f, 1.f, 250.f);
    //m_slShadowMap.PerspectiveData()->view = glm::mat4(glm::angleAxis(glm::radians(m_rad), glm::vec3(1.f, 0.f, 0.f)));
    //m_slShadowMap.PerspectiveData()->projection = glm::orthoZO(m_leftRight.x, m_leftRight.y, m_topBottom.x, m_topBottom.y, m_nearFar.x, m_nearFar.y);
    m_slShadowMap.UpdateUbo(imageIndex);
    
    m_uboViewProjection.Data.view = m_camera.GetViewMatrix();
    m_uboViewProjection.Data.projection = m_camera.GetProjectionMatrix();
    m_uboViewProjection.Data.camPosition = glm::vec4(m_camera.GetPosition(), 1.f);
    m_uboViewProjection.Data.lightSpace = m_dlShadowMap.PerspectiveData()->projection * m_dlShadowMap.PerspectiveData()->view;
    m_uboViewProjection.Data.spotLightSpace = m_slShadowMap.PerspectiveData()->projection * m_slShadowMap.PerspectiveData()->view;
    m_uboViewProjection.Update(imageIndex);
    
    m_uboPointLight.Data = m_dirLight;
    m_uboPointLight.Update(imageIndex);

    m_uboFragSettings.Update(imageIndex);
    
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
    static Object* selectedObject = nullptr;
    static bool hasUsed = false;

    glm::mat4 identity(1.f);
    glm::mat4 cameraView = m_camera.GetViewMatrix();
    glm::mat4 cameraProjection = m_camera.GetProjectionMatrix();

    glm::vec2 p1 = glm::vec4(m_objects[0]->GetPosition(), 1.0) * cameraView * cameraProjection;
    glm::vec2 p2 = glm::vec4({0.f, 0.f, 0.f, 1.f}) * cameraView * cameraProjection;
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    bool b = true;
    ImGui::Begin("Debug", &b, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    {
        ImGui::PopStyleColor();
        ImGui::SetWindowPos(ImVec2(0.f, 0.f));
        ImGui::SetWindowSize(ImVec2(m_swapchainExtent.width, m_swapchainExtent.height));

        ImGui::GetWindowDrawList()->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), 3);

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

        //ImGuizmo::DrawGrid(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), glm::value_ptr(identity), 100.f);

        if (selectedObject)
        {
            glm::mat4 transform;

            float translation[3] = { selectedObject->GetPosition().x, selectedObject->GetPosition().y, selectedObject->GetPosition().z };
            float scale[3] = { 1.f, 1.f, 1.f };
            float rotation[3] = { 0.f, 0.f, 0.f };
            ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, glm::value_ptr(transform));

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
                ImGuizmo::TRANSLATE, ImGuizmo::LOCAL,
                glm::value_ptr(transform));

            if (ImGuizmo::IsUsing())
            {
                float newPosition[3];
                float newRotation[3];
                float newScale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), newPosition, newRotation, newScale);

                selectedObject->SetPosition(glm::vec3(newPosition[0], newPosition[1], newPosition[2]));
                hasUsed = true;
            }
        }
    }
    ImGui::End();


    bool focus = !ImGuizmo::IsUsing() && hasUsed; 

    if (focus) ImGui::SetNextWindowFocus();
    ImGui::Begin("Objects");
    {
        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            bool isSelected = selectedObject == m_objects[i];
            
            if (ImGui::Selectable(m_objects[i]->Name.c_str(), isSelected))
                selectedObject = m_objects[i];
        }
    }
    ImGui::End();

    if (focus) ImGui::SetNextWindowFocus();
    ImGui::Begin("Camera");
    {
        glm::vec3 camPos = m_camera.GetPosition();
        if (ImGui::DragFloat3("Position", &camPos.x, 0.01f))
            m_camera.SetPosition(camPos);

        glm::vec3 camRot = m_camera.GetRotation();
        if (ImGui::DragFloat3("Rotation", &camRot.x, 1.f))
            m_camera.SetRotation(camRot);

        ImGui::Text("Shadow Map Camera:");

        ImGui::DragFloat("View", &m_rad);
        ImGui::DragFloat2("Left-Right", &m_leftRight.x);
        ImGui::DragFloat2("Top-Bottom", &m_topBottom.x);
        ImGui::DragFloat2("Near-Far", &m_nearFar.x);
    }
    ImGui::End();

    if (focus) ImGui::SetNextWindowFocus();
    ImGui::Begin("Directional Light");
    {
        ImGui::DragFloat3("Direction", &m_dirLight.dlDirection.x, 0.01f, -1.f, 1.f);
    }
    ImGui::End();

    if (focus) ImGui::SetNextWindowFocus();
    ImGui::Begin("Spot Light");
    {
        ImGui::DragFloat3("Position", &m_dirLight.slPosition.x, 0.01f);
        ImGui::DragFloat3("Direction", &m_dirLight.slDirection.x, 0.01f, -1.f, 1.f);
        ImGui::DragFloat("Strength", &m_dirLight.slStrength, 0.01f);
        ImGui::DragFloat("Cutoff", &m_dirLight.slCutoff, 0.01f);
    }
    ImGui::End();

    if (focus) ImGui::SetNextWindowFocus();
    ImGui::Begin("Selected Object");
    {
        if (selectedObject)
        {
            ImGui::Text("%s", selectedObject->Name.c_str());
            
            glm::vec3 position = selectedObject->GetPosition();
            if (ImGui::DragFloat3("Position", &position.x, 0.01f))
                selectedObject->SetPosition(position);
        }
        else
        {
            ImGui::Text("No object selected");
        }
    }
    ImGui::End();
    

    /*if (focus) ImGui::SetNextWindowFocus();
    ImGui::Begin("Shadow Map");
    {
        static bool drawShadowMapToScene;
        if (ImGui::Checkbox("Draw Shadowmap", &drawShadowMapToScene))
            drawShadowMapToScene ? m_uboFragSettings.Data.bDrawShadowDepth = 1 : m_uboFragSettings.Data.bDrawShadowDepth = 0;
        
        ImGui::Image(m_guiShadowMapImage, ImVec2(256, 256));
    }
    ImGui::End();*/

    if (focus)
    {
        focus = false;
        hasUsed = false;
    }
        
    //}
    //ImGui::End();

    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::EndMainMenuBar();
    }

    /*ImGui::Begin("Building");
    {
        glm::vec3 pos = m_object.GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
            m_object.SetPosition(pos);
    }
    ImGui::End();*/
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

    VkPipelineShaderStageCreateInfo shaderStages[] =
    {
        loadShader(m_device.logicalDevice, "vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        loadShader(m_device.logicalDevice, "frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };

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

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = defaultViewportFlipped(m_swapchainExtent);

    // -- RASTERIZER --

    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo =
        defaultRasterizerCreateInfo(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE);
    
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
    worldPushConstantRange.size = sizeof(PushModel);
    worldPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    worldPushConstantRange.offset = 0;
    
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &worldPushConstantRange;

    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        m_uboViewProjection.GetLayout(),
        MaterialManager::GetDescriptorSetLayout(),
        m_uboPointLight.GetLayout(),
        ShadowMap::GetDescriptorSetLayout(),
        m_uboFragSettings.GetLayout()
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

    m_guiShadowMapImage = ImGui_ImplVulkan_AddTexture(m_slShadowMap.GetSampler(), m_slShadowMap.GetAnImageView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = { 0.f, 0.f, 0.f, 1.f };
    clearValues[1].depthStencil.depth = 1.f;
    clearValues[1].depthStencil.stencil = 0;

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
        m_dlShadowMap.RecordCommands(m_commandBuffers[currentImage], currentImage, m_objects);
        m_slShadowMap.RecordCommands(m_commandBuffers[currentImage], currentImage, m_objects);
        
        vkCmdBeginRenderPass(m_commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            vkCmdBindPipeline(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            for (size_t j = 0; j < m_objects.size(); ++j)
            {
                const glm::mat4& objectTransform = m_objects[j]->GetTransform();
                std::vector<Mesh>& meshes = m_objects[j]->GetMeshes();

                for (size_t i = 0; i < meshes.size(); ++i)
                {
                    std::vector<VkDescriptorSet> descriptorSets =
                    {
                        m_uboViewProjection.GetDescriptorSet(currentImage),
                        MaterialManager::GetDescriptorSet(m_objects[j]->GetMaterialId(meshes[i].GetMaterialIndex())),
                        m_uboPointLight.GetDescriptorSet(currentImage),
                        ShadowMap::GetDescriptorSet(currentImage),
                        m_uboFragSettings.GetDescriptorSet(currentImage)
                    };
                
                    vkCmdBindDescriptorSets(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout,
                        0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
                        0, nullptr);
                
                    VkBuffer vertexBuffers[] = { meshes[i].GetVertexBuffer()->GetBuffer() };
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(m_commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);

                    PushModel pushModel = {};
                    pushModel.model = objectTransform * meshes[i].GetTransform();
                    if (m_objects[j]->Name == "Light" || m_objects[j]->Name == "Block") pushModel.shaded = 0;
                    else pushModel.shaded = 1;
                    //pushModel.shaded = 1;
                    vkCmdPushConstants(m_commandBuffers[currentImage], m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushModel), &pushModel);
                
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
