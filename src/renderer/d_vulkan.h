#pragma once

#include "tr_local.h"
#include "vulkan/vulkan.h"

typedef struct QueueFamilyIndices {
	int graphicsFamily;
	int presentFamily;
	/*
	BOOL isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0;
	}*/
} QueueFamilyIndices;

typedef struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	size_t formatCount;
	VkSurfaceFormatKHR* formats;
	size_t presentModeCount;
	VkPresentModeKHR* presentModes;
} SwapChainSupportDetails;

void d_VK_InitWindow(uint32_t width, uint32_t height, const char* title);
void d_VK_InitVulkan();
void d_VK_BeginFrame();
void d_VK_EndFrame();

void d_VK_CreateVkImage(unsigned* data,
						int32_t width, int32_t height,
						VkBool32 mipmap,
						VkBool32 picmip,
						VkBool32 lightMap,
						//int32_t* format,
						//int32_t* pUploadWidth, 
						//int32_t* pUploadHeight,
						VkBool32 noCompress,
						VkImage* result);

void d_VK_DrawTris(shaderCommands_t* input);
