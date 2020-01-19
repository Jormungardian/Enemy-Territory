#pragma once

#include <vulkan/vulkan.h>

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		assert(res == VK_SUCCESS);																		\
	}																									\
}

void d_VKUtils_CreateBuffer(VkDevice* device, VkBufferCreateInfo* bufferInfo, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkMemoryRequirements* memoryRequirements);

void d_VKUtils_CreateImage(VkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage const* image, VkDeviceMemory const* imageMemory);

VkCommandBuffer d_VKUtils_CreateCommandBuffer(VkDevice* device, VkCommandPool* commandPool, VkCommandBufferLevel level, VkBool32 begin);

void d_VKUtils_FlushCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue, VkBool32 free);