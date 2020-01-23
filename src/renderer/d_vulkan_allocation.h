#pragma once

#include <vulkan/vulkan.h>

typedef struct
{
	VkBuffer vkBuffer;
	VkDeviceMemory vkDeviceMemory;
	VkMemoryRequirements vkMemoryRequirements;
	size_t currentBufferOffset;
	size_t allocSize;
	VkBufferUsageFlags usage;
}d_VKAllocation_LargeAllocation;

void d_VKAllocation_Init(VkDevice device, d_VKAllocation_LargeAllocation* largeAlloc, size_t size, VkBufferUsageFlags usage);
VkBuffer d_VKAllocation_AcquireChunk(VkDevice device, d_VKAllocation_LargeAllocation* largeAlloc, void* inData, size_t dataSize);