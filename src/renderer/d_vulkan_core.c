#include "d_vulkan_core.h"

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

QueueFamilyIndices g_QueueFamilyIndices;

uint32_t g_WindowWidth, g_WindowHeight;

void d_VKCore_InitWindow(uint32_t width, uint32_t height, const char* title)
{
	g_WindowWidth = width;
	g_WindowHeight = height;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	g_VkWindow = glfwCreateWindow(width, height, title, NULL, NULL);
}

void d_VKCore_CreateInstance()
{
	VkApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan ET";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "idTech 3";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

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

void d_VKCore_CreateSurface()
{
	if (glfwCreateWindowSurface(g_VkInstance, g_VkWindow, NULL, &g_VkSurface) != VK_SUCCESS) {
		printf("failed to create window surface!\n");
	}
}

void d_VKCore_PickPhysicalDevice()
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

void d_VKCore_GetQueueFamilyProperties()
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

void d_VKCore_CreateLogicalDevice()
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
	deviceFeatures.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(VkDeviceCreateInfo));
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = 2;
	createInfo.pQueueCreateInfos = queueCreateInfos;
	createInfo.pEnabledFeatures = &deviceFeatures;

	// DONOTCOMMIT: Too ugly, cleanup. Works for now (for that matter this whole function needs refactor..)
	createInfo.enabledExtensionCount = 1;
	const char* extensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	const char** dext = (char**)malloc(sizeof(char**));
	dext[0] = extensions;
	createInfo.ppEnabledExtensionNames = dext;

	VkResult res = vkCreateDevice(g_VkPhysicalDevice, &createInfo, NULL, &g_VkDevice);

	if (res != VK_SUCCESS) {
		printf("Failed to create logical device!\n");
	}

	free(queueCreateInfos);

	vkGetDeviceQueue(g_VkDevice, g_QueueFamilyIndices.graphicsFamily, 0, &g_VkGraphicsQueue);
	vkGetDeviceQueue(g_VkDevice, g_QueueFamilyIndices.presentFamily, 0, &g_VkPresentQueue);
}

void d_VKCore_QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, SwapChainSupportDetails* details) {

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->formatCount, NULL);

	if (details->formatCount != 0) {
		details->formats = (VkSurfaceFormatKHR*)malloc(details->formatCount * sizeof(VkSurfaceFormatKHR));
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->formatCount, details->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->presentModeCount, NULL);

	if (details->presentModeCount != 0) {
		details->presentModes = (VkPresentModeKHR*)malloc(details->presentModeCount * sizeof(VkPresentModeKHR));
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->presentModeCount, details->presentModes);
	}
}

VkSurfaceFormatKHR d_VKCore_ChooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, size_t formatsCount) {
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

VkPresentModeKHR d_VKCore_ChooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, size_t modeCount) {
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

VkExtent2D d_VKCore_ChooseSwapExtent(VkSurfaceCapabilitiesKHR* capabilities) {
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

void d_VKCore_CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport;
	d_VKCore_QuerySwapChainSupport(g_VkPhysicalDevice, g_VkSurface, &swapChainSupport);

	VkSurfaceFormatKHR surfaceFormat = d_VKCore_ChooseSwapSurfaceFormat(swapChainSupport.formats, swapChainSupport.formatCount);
	VkPresentModeKHR presentMode = d_VKCore_ChooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.presentModeCount);
	VkExtent2D extent = d_VKCore_ChooseSwapExtent(&swapChainSupport.capabilities);

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

void d_VKCore_CreateImageViews()
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

void d_VKCore_CreateSemaphores()
{
	VkSemaphoreCreateInfo semaphoreInfo;
	memset(&semaphoreInfo, 0, sizeof(VkSemaphoreCreateInfo));
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(g_VkDevice, &semaphoreInfo, NULL, &g_ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(g_VkDevice, &semaphoreInfo, NULL, &g_RenderFinishedSemaphore) != VK_SUCCESS) {

		printf("Failed to create semaphores!");
	}
}

void d_VKCore_InitApplication()
{
	d_VKCore_CreateInstance();
	d_VKCore_CreateSurface();
	d_VKCore_PickPhysicalDevice();
	d_VKCore_GetQueueFamilyProperties();
	d_VKCore_CreateLogicalDevice();
	d_VKCore_CreateSwapChain();
	d_VKCore_CreateImageViews();
	d_VKCore_CreateSemaphores();
}
