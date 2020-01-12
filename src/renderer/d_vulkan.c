#include "d_vulkan.h"
#include "d_vulkan_utils.h"
#include "tr_local.h"
#include "GLFW/glfw3.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define INIT_STRUCT(s, n) s n; memset(&n, 0, sizeof(s));
#define IS_SUCCESS(f) assert( f != VK_SUCCESS);

GLFWwindow* g_VkWindow;

VkInstance g_VkInstance;

VkSurfaceKHR g_VkSurface;

VkPhysicalDevice g_VkPhysicalDevice;
VkDevice g_VkDevice;
VkQueue g_VkGraphicsQueue;
VkQueue g_VkPresentQueue;

uint32_t g_CurrentSwapchainImageIndex;
VkSwapchainKHR g_VkSwapchain;
size_t g_VkSwapchainImageCount;
VkImage* g_VkSwapchainImages;
VkImageView* g_VkSwapchainImageViews;
VkFormat g_VkSwapchainImageFormat;
VkExtent2D g_VkSwapchainExtent;
VkSemaphore g_ImageAvailableSemaphore;
VkSemaphore g_RenderFinishedSemaphore;

size_t g_WindowWidth;
size_t g_WindowHeight;

QueueFamilyIndices g_QueueFamilyIndices;

VkPipelineLayout g_DebugWireframePL;
VkPipeline g_DebugWireframePSO;
VkDescriptorSetLayout g_DebugDescriptorSetLayout;
VkDescriptorPool g_DebugDescriptorPool;
VkSampler g_DebugSampler;

///////////////////////////

VkRenderPass g_VkRenderPass;
VkFramebuffer* g_VkFramebuffers;
VkCommandPool g_VkCommandPool;

size_t g_VkTestCommandBuffersCount;
VkCommandBuffer* g_VkTestCommandBuffers;

VkBuffer g_VertexBuffersPoolBuffer;
VkDeviceMemory g_VertexBuffersPoolDeviceMemory;
VkMemoryRequirements g_VertexBuffersMemoryRequirements;
size_t g_CurrentVertexBufferOffset;

VkBuffer g_IndexBuffersPoolBuffer;
VkDeviceMemory g_IndexBuffersPoolDeviceMemory;
VkMemoryRequirements g_IndexBuffersMemoryRequirements;
size_t g_CurrentIndexBufferOffset;

///////////////////////////

char* ReadBinaryFile(const char* filename, size_t* outputSize)
{
	FILE* fp;
	struct stat info;
	stat(filename, &info);
	fp = fopen(filename, "rb");

	fseek(fp, 0, SEEK_END);
	*outputSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char* output = (char*)malloc(*outputSize);
	fread(output, *outputSize, 1, fp);
	fclose(fp);

	return output;
}

void CreateInstance()
{
	VkApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan ET";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "idTech 3";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	size_t deviceExtensionCount = 0;
	createInfo.ppEnabledExtensionNames = glfwGetRequiredInstanceExtensions(&deviceExtensionCount);
	createInfo.enabledExtensionCount = deviceExtensionCount;

	if (vkCreateInstance(&createInfo, NULL, &g_VkInstance) != VK_SUCCESS) {
		printf("Failed to create instance!\n");
	}
}

void CreateSurface()
{
	if (glfwCreateWindowSurface(g_VkInstance, g_VkWindow, NULL, &g_VkSurface) != VK_SUCCESS) {
		printf("failed to create window surface!\n");
	}
}

void PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(g_VkInstance, &deviceCount, NULL);

	if (deviceCount == 0) {
		printf("Failed to find GPUs with Vulkan support!\n");
	}

	VkPhysicalDevice* pPhysicalDevices = (VkPhysicalDevice*)malloc(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(g_VkInstance, &deviceCount, pPhysicalDevices);

	// Grab first device, horrible code but works. Make pretty later...
	g_VkPhysicalDevice = pPhysicalDevices[0];

	free(pPhysicalDevices);
}

void GetQueueFamilyProperties()
{
	uint32_t count = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(g_VkPhysicalDevice, &count, NULL);

	VkQueueFamilyProperties* props = (VkQueueFamilyProperties*)malloc(count * sizeof(VkQueueFamilyProperties));

	vkGetPhysicalDeviceQueueFamilyProperties(g_VkPhysicalDevice, &count, props);

	g_QueueFamilyIndices.graphicsFamily = -1;
	g_QueueFamilyIndices.presentFamily = -1;

	for (uint32_t i = 0; i < count; ++i) {
		if (props[i].queueCount > 0 && props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			g_QueueFamilyIndices.graphicsFamily = i;
		}

		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(g_VkPhysicalDevice, i, g_VkSurface, &presentSupport);

		if (props[i].queueCount > 0 && presentSupport) {
			g_QueueFamilyIndices.presentFamily = i;
		}

		if (g_QueueFamilyIndices.graphicsFamily != -1 && g_QueueFamilyIndices.presentFamily != -1) {
			break;
		}
	}

	free(props);
}

void CreateLogicalDevice()
{
	VkDeviceQueueCreateInfo* queueCreateInfos = (VkDeviceQueueCreateInfo*)malloc(2 * sizeof(VkDeviceQueueCreateInfo));

	memset(queueCreateInfos, 0, 2 * sizeof(VkDeviceQueueCreateInfo));

	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	queueCreateInfos[0].queueFamilyIndex = g_QueueFamilyIndices.graphicsFamily;
	queueCreateInfos[1].queueFamilyIndex = g_QueueFamilyIndices.presentFamily;

	float queuePriority = 1.0f;
	queueCreateInfos[0].queueCount = queueCreateInfos[1].queueCount = 1;
	queueCreateInfos[0].pQueuePriorities = queueCreateInfos[1].pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures;
	memset(&deviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
	deviceFeatures.fillModeNonSolid = TRUE;

	VkDeviceCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(VkDeviceCreateInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = 2;
	createInfo.pQueueCreateInfos = queueCreateInfos;
	createInfo.pEnabledFeatures = &deviceFeatures;

	// DONOTCOMMIT: Too ugly, cleanup. Works for now (for that matter this whole function needs refactor..)
	createInfo.enabledExtensionCount = 1;
	const char* extensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	const char** dext = (char**) malloc(sizeof(char**));
	dext[0] = extensions;
	createInfo.ppEnabledExtensionNames = dext;

	VkResult res = vkCreateDevice(g_VkPhysicalDevice, &createInfo, NULL, &g_VkDevice);

	if ( res != VK_SUCCESS) {
		printf("Failed to create logical device!\n");
	}

	free(queueCreateInfos);

	vkGetDeviceQueue(g_VkDevice, g_QueueFamilyIndices.graphicsFamily, 0, &g_VkGraphicsQueue);
	vkGetDeviceQueue(g_VkDevice, g_QueueFamilyIndices.presentFamily, 0, &g_VkPresentQueue);
}

void QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, SwapChainSupportDetails* details) {

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->formatCount, NULL);

	if (details->formatCount != 0) {
		details->formats = (VkSurfaceFormatKHR*)malloc(details->formatCount*sizeof(VkSurfaceFormatKHR));
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->formatCount, details->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->presentModeCount, NULL);

	if (details->presentModeCount != 0) {
		details->presentModes = (VkPresentModeKHR*)malloc(details->presentModeCount * sizeof(VkPresentModeKHR));
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->presentModeCount, details->presentModes);
	}
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, size_t formatsCount) {
	if (formatsCount == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		VkSurfaceFormatKHR ret;
		ret.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		ret.format = VK_FORMAT_B8G8R8A8_UNORM;
		return ret;
	}

	for (uint32_t i = 0; i < formatsCount; ++i) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats[i];
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, size_t modeCount) {
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (uint32_t i = 0; i < modeCount; ++i) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
		else if (availablePresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = availablePresentModes[i];
		}
	}

	return bestMode;
}

VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR* capabilities) {
	if (capabilities->currentExtent.width != (uint32_t)(-1)) {
		return capabilities->currentExtent;
	}
	else {
		VkExtent2D actualExtent = { g_WindowWidth, g_WindowHeight };

		// DONOTCOMMIT: FIX with actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		// DONOTCOMMIT: FIX actualExtent.height = with std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
		return actualExtent;
	}
}

void CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport;
	QuerySwapChainSupport(g_VkPhysicalDevice, g_VkSurface, &swapChainSupport);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats, swapChainSupport.formatCount);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.presentModeCount);
	VkExtent2D extent = ChooseSwapExtent(&swapChainSupport.capabilities);
	
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo;
	memset(&createInfo, 0, sizeof(VkSwapchainCreateInfoKHR));
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = g_VkSurface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { (uint32_t)g_QueueFamilyIndices.graphicsFamily, (uint32_t)g_QueueFamilyIndices.presentFamily };

	if (g_QueueFamilyIndices.graphicsFamily != g_QueueFamilyIndices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(g_VkDevice, &createInfo, NULL, &g_VkSwapchain) != VK_SUCCESS) {
		printf("Failed to create swap chain!\n");
	}

	vkGetSwapchainImagesKHR(g_VkDevice, g_VkSwapchain, &g_VkSwapchainImageCount, NULL);
	g_VkSwapchainImages = (VkImage*)malloc(g_VkSwapchainImageCount * sizeof(VkImage));
	vkGetSwapchainImagesKHR(g_VkDevice, g_VkSwapchain, &g_VkSwapchainImageCount, g_VkSwapchainImages);

	g_VkSwapchainImageFormat = surfaceFormat.format;
	g_VkSwapchainExtent = extent;
}

void CreateImageViews()
{
	g_VkSwapchainImageViews = (VkImageView*)malloc(g_VkSwapchainImageCount * sizeof(VkImageView));

	for (size_t i = 0; i < g_VkSwapchainImageCount; i++) {
		VkImageViewCreateInfo createInfo;
		memset(&createInfo, 0, sizeof(VkImageViewCreateInfo));
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = g_VkSwapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = g_VkSwapchainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(g_VkDevice, &createInfo, NULL, &g_VkSwapchainImageViews[i]) != VK_SUCCESS) {
			printf("Failed to create image views!\n");
		}
	}
}

void CreateSemaphores()
{
	VkSemaphoreCreateInfo semaphoreInfo;
	memset(&semaphoreInfo, 0, sizeof(VkSemaphoreCreateInfo));
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(g_VkDevice, &semaphoreInfo, NULL, &g_ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(g_VkDevice, &semaphoreInfo, NULL, &g_RenderFinishedSemaphore) != VK_SUCCESS) {

		printf("Failed to create semaphores!");
	}
}

void d_VK_InitWindow(uint32_t width, uint32_t height, const char* title)
{
	glfwInit();

	g_WindowWidth = width;
	g_WindowHeight = height;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	g_VkWindow = glfwCreateWindow(width, height, title, NULL, NULL);
}

void CreateRenderPass()
{
	VkAttachmentDescription colorAttachment;
	memset(&colorAttachment, 0, sizeof(VkAttachmentDescription));
	colorAttachment.format = g_VkSwapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef;
	memset(&colorAttachmentRef, 0, sizeof(VkAttachmentReference));
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	INIT_STRUCT(VkSubpassDescription, subpass)
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	INIT_STRUCT(VkSubpassDependency, dependency)
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	INIT_STRUCT(VkRenderPassCreateInfo, renderPassInfo)
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(g_VkDevice, &renderPassInfo, NULL, &g_VkRenderPass) != VK_SUCCESS) {
		printf("Failed to create render pass!\n");
	}
}

void CreateFramebuffers() 
{
	g_VkFramebuffers = (VkFramebuffer*)malloc(g_VkSwapchainImageCount * sizeof(VkFramebuffer));

	for (size_t i = 0; i < g_VkSwapchainImageCount; i++) {
		VkImageView attachments[] = {
			g_VkSwapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo;
		memset(&framebufferInfo, 0, sizeof(VkFramebufferCreateInfo));
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = g_VkRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = g_VkSwapchainExtent.width;
		framebufferInfo.height = g_VkSwapchainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(g_VkDevice, &framebufferInfo, NULL, &g_VkFramebuffers[i]) != VK_SUCCESS) {
			printf("Failed to create framebuffer!\n");
		}
	}
}

void CreateCommandPool() 
{
	VkCommandPoolCreateInfo poolInfo;
	memset(&poolInfo, 0, sizeof(VkCommandPoolCreateInfo));
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = g_QueueFamilyIndices.graphicsFamily;

	if (vkCreateCommandPool(g_VkDevice, &poolInfo, NULL, &g_VkCommandPool) != VK_SUCCESS) {
		printf("Failed to create command pool!\n");
	}
}

VkShaderModule CreateShaderModule(char* shaderCode, size_t codeSize)
{
	INIT_STRUCT(VkShaderModuleCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = codeSize;
	createInfo.pCode = (const uint32_t*)shaderCode;

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(g_VkDevice, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
		assert(0);
	}

	return shaderModule;
}

void CreateDebugDescriptorSetLayout()
{
	INIT_STRUCT(VkDescriptorSetLayoutBinding, samplerLayoutBinding);
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = &g_DebugSampler;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	INIT_STRUCT(VkDescriptorSetLayoutCreateInfo, layoutInfo);
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding;

	vkCreateDescriptorSetLayout(g_VkDevice, &layoutInfo, NULL, &g_DebugDescriptorSetLayout);
}

void CreateDebugDescriptorPool()
{
	INIT_STRUCT(VkDescriptorPoolSize, poolSize);
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = g_VkSwapchainImageCount;

	INIT_STRUCT(VkDescriptorPoolCreateInfo, poolInfo);
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1000;

	vkCreateDescriptorPool(g_VkDevice, &poolInfo, NULL, &g_DebugDescriptorPool);
}

void CreateSamplers()
{
	INIT_STRUCT(VkSamplerCreateInfo, samplerInfo);
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	vkCreateSampler(g_VkDevice, &samplerInfo, NULL, &g_DebugSampler);
}

void CreateDebugPipelines()
{
	size_t vertShaderCodeSize = 0;
	size_t fragShaderCodeSize = 0;
	char* vertShaderCode = NULL;
	char* fragShaderCode = NULL;

	vertShaderCode = ReadBinaryFile("shaders/vert.spv", &vertShaderCodeSize);
	fragShaderCode = ReadBinaryFile("shaders/frag.spv", &fragShaderCodeSize);

	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode, vertShaderCodeSize);
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode, fragShaderCodeSize);

	INIT_STRUCT(VkPipelineShaderStageCreateInfo, vertShaderStageInfo);
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	INIT_STRUCT(VkPipelineShaderStageCreateInfo, fragShaderStageInfo);
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescriptions[2];
	memset(&bindingDescriptions[0], 0, 2 * sizeof(VkVertexInputBindingDescription));
	// Vertices
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(vec4hack_t);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// UVs
	bindingDescriptions[1].binding = 1;
	bindingDescriptions[1].stride = sizeof(vec2hack_t);
	bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescriptions[2];
	memset(&attributeDescriptions[0], 0, 2 * sizeof(VkVertexInputAttributeDescription));
	// Vertices
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[0].offset = 0;
	// UVs
	attributeDescriptions[1].binding = 1;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = 0;

	INIT_STRUCT(VkPipelineVertexInputStateCreateInfo, vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 2;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions[0];
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];

	INIT_STRUCT(VkPipelineInputAssemblyStateCreateInfo, inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	INIT_STRUCT(VkViewport, viewport);
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)g_VkSwapchainExtent.width;
	viewport.height = (float)g_VkSwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	INIT_STRUCT(VkRect2D, scissor);
	scissor.offset.x = 0; 
	scissor.offset.y = 0;
	scissor.extent = g_VkSwapchainExtent;

	INIT_STRUCT(VkPipelineViewportStateCreateInfo, viewportState);
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	INIT_STRUCT(VkPipelineRasterizationStateCreateInfo, rasterizer);
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	INIT_STRUCT(VkPipelineMultisampleStateCreateInfo, multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	INIT_STRUCT(VkPipelineColorBlendAttachmentState, colorBlendAttachment);
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	INIT_STRUCT(VkPipelineColorBlendStateCreateInfo, colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPushConstantRange pushConstantRange;
	pushConstantRange.offset = 0;
	pushConstantRange.size = 6 * sizeof(float);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	INIT_STRUCT(VkPipelineLayoutCreateInfo, pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &g_DebugDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(g_VkDevice, &pipelineLayoutInfo, NULL, &g_DebugWireframePL) != VK_SUCCESS) {
		assert(0);
	}

	INIT_STRUCT(VkGraphicsPipelineCreateInfo, pipelineInfo);
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = g_DebugWireframePL;
	pipelineInfo.renderPass = g_VkRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(g_VkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &g_DebugWireframePSO) != VK_SUCCESS) {
		assert(0);
	}

	vkDestroyShaderModule(g_VkDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(g_VkDevice, vertShaderModule, NULL);
}

void CreateTestCommandBuffers()
{
	g_VkTestCommandBuffersCount = g_VkSwapchainImageCount;
	g_VkTestCommandBuffers = (VkCommandBuffer*)malloc(g_VkTestCommandBuffersCount * sizeof(VkCommandBuffer));

	INIT_STRUCT(VkCommandBufferAllocateInfo, allocInfo)
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = g_VkCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 3;

	if (vkAllocateCommandBuffers(g_VkDevice, &allocInfo, g_VkTestCommandBuffers) != VK_SUCCESS) {
		printf("Failed to allocate test command buffer!\n");
	}
}

void ConstructTestCommandBuffers()
{
	for (uint32_t i = 0; i < g_VkTestCommandBuffersCount; ++i) {
		INIT_STRUCT(VkCommandBufferBeginInfo, beginInfo)
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (vkBeginCommandBuffer(g_VkTestCommandBuffers[i], &beginInfo) != VK_SUCCESS) {
			printf("Failed to record command buffer!\n");
		}

		INIT_STRUCT(VkRenderPassBeginInfo, renderPassInfo)
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = g_VkRenderPass;
		renderPassInfo.framebuffer = g_VkFramebuffers[i];
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = g_VkSwapchainExtent;

		VkClearValue clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(g_VkTestCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		//// TODO

		vkCmdEndRenderPass(g_VkTestCommandBuffers[i]);

		if (vkEndCommandBuffer(g_VkTestCommandBuffers[i]) != VK_SUCCESS) {
			printf("Failed to record command buffer!\n");
		}
	}
}

void d_VK_InitVulkan()
{
	CreateInstance();
	CreateSurface();
	PickPhysicalDevice();
	GetQueueFamilyProperties();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateSemaphores();

	CreateRenderPass();
	CreateFramebuffers();
	CreateCommandPool();

	CreateSamplers();
	CreateDebugDescriptorPool();
	CreateDebugDescriptorSetLayout();
	CreateDebugPipelines();


	CreateTestCommandBuffers();

	{
		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = 68435456;  // Allocate pool for vertex buffers
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		d_VK_CreateBuffer(&g_VkDevice, &bufferInfo, &g_VertexBuffersPoolBuffer, &g_VertexBuffersPoolDeviceMemory, &g_VertexBuffersMemoryRequirements);

		g_CurrentVertexBufferOffset = 0;
	}

	{
		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = 68435456;  // Allocate pool for index buffers
		bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		d_VK_CreateBuffer(&g_VkDevice, &bufferInfo, &g_IndexBuffersPoolBuffer, &g_IndexBuffersPoolDeviceMemory, &g_IndexBuffersMemoryRequirements);

		g_CurrentIndexBufferOffset = 0;
	}

	//ConstructTestCommandBuffers();
	//CreateGraphicsPipeline();
	/*
	CreateVertexBuffer();
	CreateCommandBuffers();*/
}

void d_VK_BeginFrame()
{
	vkAcquireNextImageKHR(g_VkDevice, g_VkSwapchain, (uint64_t)(-1), g_ImageAvailableSemaphore, VK_NULL_HANDLE, &g_CurrentSwapchainImageIndex);

	g_CurrentVertexBufferOffset = 0;
	g_CurrentIndexBufferOffset = 0;


	// Begin command buffer recording for this swapchain image index
	INIT_STRUCT(VkCommandBufferBeginInfo, beginInfo);
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], &beginInfo) != VK_SUCCESS) {
		printf("Failed to record command buffer!\n");
	}

	INIT_STRUCT(VkRenderPassBeginInfo, renderPassInfo);
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = g_VkRenderPass;
	renderPassInfo.framebuffer = g_VkFramebuffers[g_CurrentSwapchainImageIndex];
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent = g_VkSwapchainExtent;

	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	//// TODO
}

void d_VK_EndFrame()
{
	// End recording of current command buffer
	vkCmdEndRenderPass(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex]);
	if (vkEndCommandBuffer(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex]) != VK_SUCCESS) {
		printf("Failed to record command buffer!\n");
	}

	// Submit command buffer
	INIT_STRUCT(VkSubmitInfo, submitInfo)
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { g_ImageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex];

	VkSemaphore signalSemaphores[] = { g_RenderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(g_VkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		printf("Failed to submit command!\n");
	}

	VkPresentInfoKHR presentInfo;
	memset(&presentInfo, 0, sizeof(VkPresentInfoKHR));
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { g_VkSwapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &g_CurrentSwapchainImageIndex;

	vkQueuePresentKHR(g_VkPresentQueue, &presentInfo);

	vkQueueWaitIdle(g_VkPresentQueue);
}

uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(g_VkPhysicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}

	assert(0);
	return 0;
}

void d_VK_CreateVkImage(unsigned* pixels,
	int32_t width, 
	int32_t height,
	VkBool32 mipmap,
	VkBool32 picmip,
	VkBool32 lightMap,
	//int32_t* format,
	//int32_t* pUploadWidth, 
	//int32_t* pUploadHeight,
	VkBool32 noCompress,
	VkImage* vkImage,
	VkDeviceMemory* vkDeviceMemory,
	VkImageView* vkImageView,
	VkDescriptorSet* vkDescriptorSet)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	VkMemoryRequirements stagingReqs;
	size_t textureSize = width * height * 4;
	
	// Create a host-visible staging buffer that contains the raw image data
	INIT_STRUCT(VkBufferCreateInfo, bufferCI);
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.size = textureSize;
	bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	d_VK_CreateBuffer(&g_VkDevice, &bufferCI, &stagingBuffer, &stagingMemory, &stagingReqs);

	void* data;
	vkMapMemory(g_VkDevice, stagingMemory, 0, textureSize, 0, &data);
	memcpy(data, pixels, textureSize);
	vkUnmapMemory(g_VkDevice, stagingMemory);

	d_Vk_CreateImage(&g_VkDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vkImage, vkDeviceMemory);

	VkCommandBuffer copyCmd = d_VK_CreateCommandBuffer(&g_VkDevice, &g_VkCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_TRUE);

	INIT_STRUCT(VkImageSubresourceRange, subresourceRange);
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	INIT_STRUCT(VkImageMemoryBarrier, imageMemoryBarrier);
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = *vkImage;
	imageMemoryBarrier.subresourceRange = subresourceRange;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	vkCmdPipelineBarrier(
		copyCmd,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);

	INIT_STRUCT(VkBufferImageCopy, bufferCopyRegion);
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = width;
	bufferCopyRegion.imageExtent.height = height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		*vkImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion);

	// Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
	// Source pipeline stage stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
	// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
	vkCmdPipelineBarrier(
		copyCmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);

	d_VK_FlushCommandBuffer(g_VkDevice, g_VkCommandPool, copyCmd, g_VkGraphicsQueue, VK_TRUE);

	// Clean up staging resources
	vkFreeMemory(g_VkDevice, stagingMemory, NULL);
	vkDestroyBuffer(g_VkDevice, stagingBuffer, NULL);

	// Create ImageView
	INIT_STRUCT(VkImageViewCreateInfo, viewInfo);
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *vkImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	vkCreateImageView(g_VkDevice, &viewInfo, NULL, vkImageView);

	// Create Descriptor Set for this image,
	// DONOTCOMMIT: Is there a better way?
	{
		VkDescriptorSet ds;
		VkDescriptorSetLayout layout[] = { g_DebugDescriptorSetLayout };
		INIT_STRUCT(VkDescriptorSetAllocateInfo, allocInfo);
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = g_DebugDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout[0];
		vkAllocateDescriptorSets(g_VkDevice, &allocInfo, vkDescriptorSet);

		INIT_STRUCT(VkDescriptorImageInfo, imageInfo);
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = *vkImageView;
		imageInfo.sampler = &g_DebugSampler;

		INIT_STRUCT(VkWriteDescriptorSet, descWrite);
		descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrite.dstSet = *vkDescriptorSet;
		descWrite.dstBinding = 0;
		descWrite.dstArrayElement = 0;
		descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descWrite.descriptorCount = 1;
		descWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(g_VkDevice, 1, &descWrite, 0, NULL);
	}
}

void d_VK_DrawTris(shaderCommands_t* input)
{
	VkBuffer vertexBuffer;
	VkBuffer uvsBuffer;
	VkBuffer indexBuffer;

	{
		// Vertices

		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(vec4hack_t) * input->numVertexes;
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

#if 1
		// USE POOL

		if (vkCreateBuffer(g_VkDevice, &bufferInfo, NULL, &vertexBuffer) != VK_SUCCESS) {
			assert(0);
		}
		
		vkBindBufferMemory(g_VkDevice, vertexBuffer, g_VertexBuffersPoolDeviceMemory, g_CurrentVertexBufferOffset);
		void* data;
		vkMapMemory(g_VkDevice, g_VertexBuffersPoolDeviceMemory, g_CurrentVertexBufferOffset, bufferInfo.size, 0, &data);
		memcpy(data, input->xyz, (size_t)bufferInfo.size);
		vkUnmapMemory(g_VkDevice, g_VertexBuffersPoolDeviceMemory);

		g_CurrentVertexBufferOffset += sizeof(vec4hack_t) * input->numVertexes;

		while (g_CurrentVertexBufferOffset % g_VertexBuffersMemoryRequirements.alignment != 0) {
			g_CurrentVertexBufferOffset++;
		}
#else
		
		VkDeviceMemory deviceMemory;
		VkMemoryRequirements memReqs;

		d_VK_CreateBuffer(&g_VkDevice, &bufferInfo, &vertexBuffer, &deviceMemory, &memReqs);

		void* data;
		vkMapMemory(g_VkDevice, deviceMemory, 0, bufferInfo.size, 0, &data);
		memcpy(data, input->xyz, (size_t)bufferInfo.size);
		vkUnmapMemory(g_VkDevice, deviceMemory);
#endif
	}

	{
		// UVs

		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(vec2hack_t) * input->numVertexes;
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(g_VkDevice, &bufferInfo, NULL, &uvsBuffer) != VK_SUCCESS) {
			assert(0);
		}

		vkBindBufferMemory(g_VkDevice, uvsBuffer, g_VertexBuffersPoolDeviceMemory, g_CurrentVertexBufferOffset);
		void* data;
		vkMapMemory(g_VkDevice, g_VertexBuffersPoolDeviceMemory, g_CurrentVertexBufferOffset, bufferInfo.size, 0, &data);
		memcpy(data, input->texCoords0, (size_t)bufferInfo.size);
		vkUnmapMemory(g_VkDevice, g_VertexBuffersPoolDeviceMemory);

		g_CurrentVertexBufferOffset += sizeof(vec2hack_t) * input->numVertexes;

		while (g_CurrentVertexBufferOffset % g_VertexBuffersMemoryRequirements.alignment != 0) {
			g_CurrentVertexBufferOffset++;
		}
	}

	{
		// Indexes

		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = (uint64_t)(sizeof(int) * input->numIndexes);
		bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

#if 1
		// USE POOL

		if (vkCreateBuffer(g_VkDevice, &bufferInfo, NULL, &indexBuffer) != VK_SUCCESS) {
			assert(0);
		}

		vkBindBufferMemory(g_VkDevice, indexBuffer, g_IndexBuffersPoolDeviceMemory, g_CurrentIndexBufferOffset);
		void* data;
		vkMapMemory(g_VkDevice, g_IndexBuffersPoolDeviceMemory, g_CurrentIndexBufferOffset, bufferInfo.size, 0, &data);
		memcpy(data, input->indexes, (size_t)bufferInfo.size);
		vkUnmapMemory(g_VkDevice, g_IndexBuffersPoolDeviceMemory);

		g_CurrentIndexBufferOffset += sizeof(int) * input->numIndexes;

		while (g_CurrentIndexBufferOffset % g_IndexBuffersMemoryRequirements.alignment != 0) {
			g_CurrentIndexBufferOffset++;
		}
#else
		VkDeviceMemory deviceMemory;
		VkMemoryRequirements memReqs;

		d_VK_CreateBuffer(&g_VkDevice, &bufferInfo, &indexBuffer, &deviceMemory, &memReqs);

		void* data;
		vkMapMemory(g_VkDevice, deviceMemory, 0, bufferInfo.size, 0, &data);
		memcpy(data, input->indexes, (size_t)bufferInfo.size);
		vkUnmapMemory(g_VkDevice, deviceMemory);
#endif
	}

	VkBuffer vertexBuffers[] = { vertexBuffer, uvsBuffer };
	VkDeviceSize offsets[] = { 0, 0 };
	vkCmdBindVertexBuffers(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], 0, 2, &vertexBuffers[0], offsets);

	vkCmdBindIndexBuffer(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	if (backEnd.projection2D == qtrue) {
		float ortho[6] = { glConfig.vidWidth, 0.0, glConfig.vidHeight, 0.0, 1.0, -1.0 };
		vkCmdBindPipeline(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_DebugWireframePSO);
		vkCmdPushConstants(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], g_DebugWireframePL, VK_SHADER_STAGE_VERTEX_BIT, 0, 6 * sizeof(float), &ortho[0]);

		{
			//if (input->shader->numUnfoggedPasses > 0 && input->shader->stages[0]->bundle[0] != NULL && input->shader->stages[0]->bundle[0].image != NULL) 
			{
				image_t* img = input->shader->stages[0]->bundle[0].image[0];

				vkCmdBindDescriptorSets(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, g_DebugWireframePL, 0, 1, &img->vkDescriptorSet, 0, NULL);
			}
		}

		vkCmdDrawIndexed(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], input->numIndexes, 1, 0, 0, 0);
	}
	else {
		// TODO: Create a PSO for 3D
	}
	
	//vkCmdDraw(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], input->numVertexes, 1, 0, 0);
}
