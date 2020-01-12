#include "d_vulkan_utils.h"
#include <vulkan/vulkan.h>
#include <assert.h>

#define INIT_STRUCT(s, n) s n; memset(&n, 0, sizeof(s));
#define IS_SUCCESS(f) assert( f != VK_SUCCESS);

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

void d_Vk_CreateImage(VkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage const* image, VkDeviceMemory const* imageMemory, VkImageView const* imageView) {
	INIT_STRUCT(VkImageCreateInfo, imageInfo);
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vkCreateImage(*device, &imageInfo, NULL, image);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*device, *image, &memRequirements);

	INIT_STRUCT(VkMemoryAllocateInfo, allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	vkAllocateMemory(*device, &allocInfo, NULL, imageMemory);

	vkBindImageMemory(*device, *image, *imageMemory, 0);
}

inline VkCommandBufferAllocateInfo commandBufferAllocateInfo(
	VkCommandPool commandPool,
	VkCommandBufferLevel level,
	uint32_t bufferCount)
{
	INIT_STRUCT(VkCommandBufferAllocateInfo, commandBufferAllocateInfo);
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = level;
	commandBufferAllocateInfo.commandBufferCount = bufferCount;
	return commandBufferAllocateInfo;
}

VkCommandBuffer d_VK_CreateCommandBuffer(VkDevice* device, VkCommandPool* commandPool, VkCommandBufferLevel level, VkBool32 begin)
{
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = commandBufferAllocateInfo(*commandPool, level, 1);

	VkCommandBuffer cmdBuffer;
	vkAllocateCommandBuffers(*device, &cmdBufAllocateInfo, &cmdBuffer);

	// If requested, also start recording for the new command buffer
	if (begin)
	{
		INIT_STRUCT(VkCommandBufferBeginInfo, cmdBufInfo);
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo);
	}

	return cmdBuffer;
}

void d_VK_FlushCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue, VkBool32 free)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	vkEndCommandBuffer(commandBuffer);

	INIT_STRUCT(VkSubmitInfo, submitInfo);
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	INIT_STRUCT(VkFenceCreateInfo, fenceInfo);
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence;
	vkCreateFence(device, &fenceInfo, NULL, &fence);

	// Submit to the queue
	vkQueueSubmit(queue, 1, &submitInfo, fence);
	// Wait for the fence to signal that command buffer has finished executing
	vkWaitForFences(device, 1, &fence, VK_TRUE, (uint64_t)(-1));

	vkDestroyFence(device, fence, NULL);

	if (free)
	{
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}
}