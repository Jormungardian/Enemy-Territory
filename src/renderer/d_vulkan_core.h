#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define INIT_STRUCT(s, n) s n; memset(&n, 0, sizeof(s));

typedef struct QueueFamilyIndices {
	int graphicsFamily;
	int presentFamily;
} QueueFamilyIndices;

typedef struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	size_t formatCount;
	VkSurfaceFormatKHR* formats;
	size_t presentModeCount;
	VkPresentModeKHR* presentModes;
} SwapChainSupportDetails;

extern VkInstance g_VkInstance;

extern VkSurfaceKHR g_VkSurface;

extern VkPhysicalDevice g_VkPhysicalDevice;
extern VkDevice g_VkDevice;
extern VkQueue g_VkGraphicsQueue;
extern VkQueue g_VkPresentQueue;

extern uint32_t g_CurrentSwapchainImageIndex;
extern VkSwapchainKHR g_VkSwapchain;
extern size_t g_VkSwapchainImageCount;
extern VkImage* g_VkSwapchainImages;
extern VkImageView* g_VkSwapchainImageViews;
extern VkFormat g_VkSwapchainImageFormat;
extern VkExtent2D g_VkSwapchainExtent;
extern VkSemaphore g_ImageAvailableSemaphore;
extern VkSemaphore g_RenderFinishedSemaphore;

extern QueueFamilyIndices g_QueueFamilyIndices;

void d_VKCore_InitWindow(uint32_t width, uint32_t height, const char* title);
void d_VKCore_InitApplication();
