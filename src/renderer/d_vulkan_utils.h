#pragma once

#include <vulkan/vulkan.h>

void d_VK_CreateBuffer(VkDevice* device, VkBufferCreateInfo* bufferInfo, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkMemoryRequirements* memoryRequirements);

void d_Vk_CreateImage(VkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage const* image, VkDeviceMemory const* imageMemory);

VkCommandBuffer d_VK_CreateCommandBuffer(VkDevice* device, VkCommandPool* commandPool, VkCommandBufferLevel level, VkBool32 begin);

void d_VK_FlushCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue, VkBool32 free);