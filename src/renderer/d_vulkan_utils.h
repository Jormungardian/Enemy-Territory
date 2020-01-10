#pragma once

#include <vulkan/vulkan.h>

void d_VK_CreateBuffer(VkDevice* device, VkBufferCreateInfo* bufferInfo, VkBuffer* buffer, VkDeviceMemory* deviceMemory, VkMemoryRequirements* memoryRequirements);