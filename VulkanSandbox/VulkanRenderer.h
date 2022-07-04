#pragma once

#define VK_DEBUG

#include <vector>

#include "Camera.h"
#include "HeightMapObject.h"
#include "Utilities.h"
#include "Image.h"
#include "Mesh.h"
#include "Object.h"
#include "ShadowMap.h"
#include "UniformBuffer.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();
	
	void Init();
	void Update(float deltaTime);
	void Draw();
	void Destroy();
	
private:
	const int MAX_CONCURRENT_FRAMES = 3;

	HeightMapObject m_terrain;

	Mesh m_testMesh;
	Camera m_camera;

	float m_rad = 45.f;
	glm::vec2 m_leftRight = { -1000.f, 1000.f };
	glm::vec2 m_topBottom = { -1000.f, 1000.f };
	glm::vec2 m_nearFar = { -1000.f, 1000.f };

	// Scene
	std::vector<Object*> m_objects;
	UboDirLight m_dirLight;
	UniformBuffer<UboDirLight> m_uboPointLight;
	
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
	VkDescriptorSet m_guiShadowMapImage;
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
	
	UniformBuffer<UboViewProjection> m_uboViewProjection;
	UniformBuffer<UboFragSettings> m_uboFragSettings;
	
	void initImGui();
	
	void recordCommands(uint32_t currentImage);

	// Shadow Mapping
	ShadowMap m_dlShadowMap;
	ShadowMap m_slShadowMap;
	
	// Get Functions
	void getPhysicalDevice();
	SwapChainDetails getSwapchainDetails(VkPhysicalDevice device);

	// -- Create Functions --
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

	void(*cmdSetPrimitiveTopologyEXT)(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology);
	
};

