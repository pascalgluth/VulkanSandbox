#pragma once

#define VK_DEBUG

#include <vector>

#include "Camera.h"
#include "Utilities.h"
#include "Image.h"
#include "Mesh.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();
	
	void Init();
	void Draw();
	void Destroy();
	
private:

	const int MAX_CONCURRENT_FRAMES = 3;

	Mesh m_testMesh;
	Camera m_camera;
	
	// Vulkan Components
	
	VkInstance m_instance;
	VkSurfaceKHR m_surface;
	struct 
	{
		VkDevice logicalDevice;
		VkPhysicalDevice physicalDevice;
	} m_device;
	VkQueue m_graphicsQueue;
	VkQueue m_presentationQueue;
	VkSwapchainKHR m_swapchain;

	VkFormat m_swapchainImageFormat;
	VkExtent2D m_swapchainExtent;
	std::vector<SwapChainImage> m_swapchainImages;
	std::vector<VkFramebuffer> m_swapchainFrameBuffers;

	VkFormat m_depthBufferImageFormat;
	std::vector<Image> m_depthBufferImage;

	VkPipeline m_graphicsPipeline;
	VkPipelineLayout m_graphicsPipelineLayout;

	VkRenderPass m_renderPass;

	VkCommandPool m_graphicsCommandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;

	std::vector<VkSemaphore> m_imageAvailable;
	std::vector<VkSemaphore> m_renderFinished;
	std::vector<VkFence> m_waitForDrawFinished;

	// MSAA
	VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	std::vector<Image> m_colorResolveImage;

	// ImGui
	VkDescriptorPool m_imguiDescriptorPool;
	void renderImGui();

	// -- Vulkan Functions --

	// Create Functions
	void createInstance();
	void createWindowSurface();
	void createLogicalDevice();
	void createSwapchain();
	void createPipeline();
	void createDepthBufferImage();
	void createFrameBuffers();
	void createRenderPass();
	void createCommandPool();
	void createGraphicsCommandBuffer();
    
    void test();

	void initImGui();
	
	void recordCommands(uint32_t currentImage);

	// Get Functions
	void getPhysicalDevice();
	SwapChainDetails getSwapchainDetails(VkPhysicalDevice device);

	// -- Create Functions --
	VkShaderModule createShaderModule(const std::vector<char>& spv);
	void createSynchronization();

	// -- Support Functions --
	bool checkInstanceExtensionSupport(const std::vector<const char*>& extensions);
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& modes);
	VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	// -- Validation --
#ifdef VK_DEBUG
	const bool m_enableValidationLayers = true;
#else
	const bool m_enableValidationLayers = false;
#endif
	const std::vector<const char*> m_validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	bool checkValidationLayerSupport();
	
};

