#include "d_vulkan_allocation.h"
#include "d_vulkan_core.h"
#include "d_vulkan_utils.h"
#include <vulkan/vulkan.h>
#include <assert.h>

void d_VKAllocation_Init(VkDevice device, d_VKAllocation_LargeAllocation* largeAlloc, size_t size, VkBufferUsageFlags usage)
{
	if (largeAlloc != NULL) {
		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		d_VKUtils_CreateBuffer(&device, &bufferInfo, &largeAlloc->vkBuffer, &largeAlloc->vkDeviceMemory, &largeAlloc->vkMemoryRequirements);

		largeAlloc->currentBufferOffset = 0;
		largeAlloc->allocSize = size;
		largeAlloc->usage = usage;
	}
}

VkBuffer d_VKAllocation_AcquireChunk(VkDevice device, d_VKAllocation_LargeAllocation* largeAlloc, void* inData, size_t dataSize)
{
	if (largeAlloc != NULL) {
		VkBuffer ret;

		INIT_STRUCT(VkBufferCreateInfo, bufferInfo);
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = dataSize;
		bufferInfo.usage = largeAlloc->usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(g_VkDevice, &bufferInfo, NULL, &ret));

		VK_CHECK_RESULT(vkBindBufferMemory(device, ret, largeAlloc->vkDeviceMemory, largeAlloc->currentBufferOffset));

		void* data;
		vkMapMemory(g_VkDevice, largeAlloc->vkDeviceMemory, largeAlloc->currentBufferOffset, bufferInfo.size, 0, &data);
		memcpy(data, inData, (size_t)bufferInfo.size);
		vkUnmapMemory(g_VkDevice, largeAlloc->vkDeviceMemory);

		largeAlloc->currentBufferOffset += dataSize;

		while (largeAlloc->currentBufferOffset % largeAlloc->vkMemoryRequirements.alignment != 0) {
			largeAlloc->currentBufferOffset++;
		}

		return ret;
	}

	return VK_NULL_HANDLE;
}