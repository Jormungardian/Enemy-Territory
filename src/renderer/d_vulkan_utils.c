#include "d_vulkan_utils.h"
#include <vulkan/vulkan.h>
#include <assert.h>

#define INIT_STRUCT(s, n) s n; memset(&n, 0, sizeof(s));

void d_VK_CreateBuffer(VkDevice* device, VkBufferCreateInfo* bufferInfo, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkMemoryRequirements* memRequirements)
{
	if (vkCreateBuffer(*device, bufferInfo, NULL, buffer) != VK_SUCCESS) {
		assert(0);
	}

	//VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(*device, *buffer, memRequirements);

	INIT_STRUCT(VkMemoryAllocateInfo, allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements->size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements->memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(*device, &allocInfo, NULL, deviceMemory) != VK_SUCCESS) {
		assert(0);
	}

	vkBindBufferMemory(*device, *buffer, *deviceMemory, 0);
}