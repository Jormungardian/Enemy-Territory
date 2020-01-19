#pragma once

#include "tr_local.h"
#include "vulkan/vulkan.h"

void d_VK_InitWindow(uint32_t width, uint32_t height, const char* title);
void d_VK_InitVulkan();
void d_VK_BeginFrame();
void d_VK_EndFrame();

void d_VK_CreateImage(unsigned* pixels,
						int32_t width, int32_t height,
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
						VkDescriptorSet* vkDescriptorSet);

void d_VK_DestroyImage(image_t* image);

void d_VK_DrawTris(shaderCommands_t* input, uint32_t stage);
