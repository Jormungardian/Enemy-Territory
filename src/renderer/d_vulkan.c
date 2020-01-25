#include "d_vulkan.h"
#include "d_vulkan_utils.h"
#include "d_vulkan_core.h"
#include "d_vulkan_allocation.h"
#include "tr_local.h"
#include "GLFW/glfw3.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "cglm/cglm.h"

#include "3rdParty/map/src/map.h"

VkPipelineLayout g_OrthoPipelineLayout;
VkPipelineLayout g_PerspPipelineLayout;
VkDescriptorSetLayout g_DebugDescriptorSetLayout;
VkDescriptorPool g_DebugDescriptorPool;
VkSampler g_DebugSampler;

VkDescriptorSet g_DrawDebugDepthRTDescriptorSet;

VkImageView g_testDepth;

///////////////////////////

VkRenderPass g_VkRenderPass;
VkFramebuffer* g_VkFramebuffers;
VkCommandPool g_VkCommandPool;

size_t g_VkTestCommandBuffersCount;
VkCommandBuffer* g_VkTestCommandBuffers;

#define D_VK_BUFFER_POOL_SIZE 4096
size_t g_CurrentBufferPoolIndex = 0;
VkBuffer g_BufferPool[D_VK_BUFFER_POOL_SIZE];

d_VKAllocation_LargeAllocation g_VertexAttribsAllocation;
d_VKAllocation_LargeAllocation g_IndexAllocation;

map_void_t g_Q3ShaderMap;

VkShaderModule g_OrthoVertShaderModule;
VkShaderModule g_PerspVertShaderModule;
VkShaderModule g_OrthoFragShaderModule;
VkShaderModule g_DepthRTFragShaderModule;

///////////////////////////

char* ReadBinaryFile(const char* filename, size_t* outputSize)
{
	FILE* fp;
	struct stat info;
	stat(filename, &info);
	fp = fopen(filename, "rb");

	fseek(fp, 0, SEEK_END);
	*outputSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char* output = (char*)malloc(*outputSize);
	fread(output, *outputSize, 1, fp);
	fclose(fp);

	return output;
}

void d_VK_InitWindow(uint32_t width, uint32_t height, const char* title)
{
	d_VKCore_InitWindow(width, height, title);
}

void CreateRenderPass()
{
	VkAttachmentDescription colorAttachment;
	memset(&colorAttachment, 0, sizeof(VkAttachmentDescription));
	colorAttachment.format = g_VkSwapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	INIT_STRUCT(VkAttachmentDescription, depthAttachment);
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	INIT_STRUCT(VkAttachmentReference, colorAttachmentRef);
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	INIT_STRUCT(VkAttachmentReference, depthAttachmentRef);
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	INIT_STRUCT(VkSubpassDescription, subpass)
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	INIT_STRUCT(VkSubpassDependency, dependency);
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachmentDescriptions[] = { colorAttachment, depthAttachment };
	INIT_STRUCT(VkRenderPassCreateInfo, renderPassInfo)
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = &attachmentDescriptions[0];
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VK_CHECK_RESULT(vkCreateRenderPass(g_VkDevice, &renderPassInfo, NULL, &g_VkRenderPass));
}

void CreateFramebuffers() 
{
	VkImage depthImage;
	VkDeviceMemory depthMemory;

	d_VKUtils_CreateImage(&g_VkDevice, g_VkSwapchainExtent.width, g_VkSwapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthImage, &depthMemory);
	INIT_STRUCT(VkImageViewCreateInfo, depthViewCI);
	depthViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthViewCI.image = depthImage;
	depthViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthViewCI.format = VK_FORMAT_D32_SFLOAT;
	depthViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthViewCI.subresourceRange.baseMipLevel = 0;
	depthViewCI.subresourceRange.levelCount = 1;
	depthViewCI.subresourceRange.baseArrayLayer = 0;
	depthViewCI.subresourceRange.layerCount = 1;
	VK_CHECK_RESULT(vkCreateImageView(g_VkDevice, &depthViewCI, NULL, &g_testDepth));

	g_VkFramebuffers = (VkFramebuffer*)malloc(g_VkSwapchainImageCount * sizeof(VkFramebuffer));

	for (size_t i = 0; i < g_VkSwapchainImageCount; i++) {
		VkImageView attachments[] = {
			g_VkSwapchainImageViews[i],
			g_testDepth
		};

		VkFramebufferCreateInfo framebufferInfo;
		memset(&framebufferInfo, 0, sizeof(VkFramebufferCreateInfo));
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = g_VkRenderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = &attachments[0];
		framebufferInfo.width = g_VkSwapchainExtent.width;
		framebufferInfo.height = g_VkSwapchainExtent.height;
		framebufferInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(g_VkDevice, &framebufferInfo, NULL, &g_VkFramebuffers[i]));
	}
}

void CreateCommandPool() 
{
	INIT_STRUCT(VkCommandPoolCreateInfo, poolInfo);
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = g_QueueFamilyIndices.graphicsFamily;

	VK_CHECK_RESULT(vkCreateCommandPool(g_VkDevice, &poolInfo, NULL, &g_VkCommandPool));
}

VkShaderModule CreateShaderModule(char* shaderCode, size_t codeSize)
{
	INIT_STRUCT(VkShaderModuleCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = codeSize;
	createInfo.pCode = (const uint32_t*)shaderCode;

	VkShaderModule shaderModule;
	VK_CHECK_RESULT(vkCreateShaderModule(g_VkDevice, &createInfo, NULL, &shaderModule));

	return shaderModule;
}

void CreateShaderModules()
{
	size_t vertOrthoShaderCodeSize = 0;
	size_t vertPerspShaderCodeSize = 0;
	size_t fragShaderCodeSize = 0;
	size_t depthRTCodeSize = 0;
	char* vertOrthoShaderCode = NULL;
	char* vertPerspShaderCode = NULL;
	char* fragShaderCode = NULL;
	char* depthRTShaderCode = NULL;

	vertOrthoShaderCode = ReadBinaryFile("shaders/vert_ortho.spv", &vertOrthoShaderCodeSize);
	vertPerspShaderCode = ReadBinaryFile("shaders/vert_persp.spv", &vertPerspShaderCodeSize);
	fragShaderCode = ReadBinaryFile("shaders/frag.spv", &fragShaderCodeSize);
	depthRTShaderCode = ReadBinaryFile("shaders/depth_rt.spv", &depthRTCodeSize);

	g_OrthoVertShaderModule = CreateShaderModule(vertOrthoShaderCode, vertOrthoShaderCodeSize);
	g_PerspVertShaderModule = CreateShaderModule(vertPerspShaderCode, vertPerspShaderCodeSize);
	g_OrthoFragShaderModule = CreateShaderModule(fragShaderCode, fragShaderCodeSize);
	g_DepthRTFragShaderModule = CreateShaderModule(depthRTShaderCode, depthRTCodeSize);

	free(vertOrthoShaderCode);
	free(vertPerspShaderCode);
	free(fragShaderCode);
	free(depthRTShaderCode);
}

void CreateDebugDescriptorSetLayout()
{
	INIT_STRUCT(VkDescriptorSetLayoutBinding, samplerLayoutBinding);
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = &g_DebugSampler;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	INIT_STRUCT(VkDescriptorSetLayoutCreateInfo, layoutInfo);
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding;

	vkCreateDescriptorSetLayout(g_VkDevice, &layoutInfo, NULL, &g_DebugDescriptorSetLayout);
}

VkDescriptorSet CreateDrawDebugRTDescriptorSet(VkImageView imageView)
{
	VkDescriptorSet ret;
	VkDescriptorSetLayout layout[] = { g_DebugDescriptorSetLayout };
	INIT_STRUCT(VkDescriptorSetAllocateInfo, allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = g_DebugDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout[0];
	vkAllocateDescriptorSets(g_VkDevice, &allocInfo, &ret);

	INIT_STRUCT(VkDescriptorImageInfo, imageInfo);
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	imageInfo.imageView = imageView;
	imageInfo.sampler = g_DebugSampler;

	INIT_STRUCT(VkWriteDescriptorSet, descWrite);
	descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrite.dstSet = ret;
	descWrite.dstBinding = 0;
	descWrite.dstArrayElement = 0;
	descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descWrite.descriptorCount = 1;
	descWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(g_VkDevice, 1, &descWrite, 0, NULL);

	return ret;
}

void CreateDebugDescriptorPool()
{
	INIT_STRUCT(VkDescriptorPoolSize, poolSize);
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = g_VkSwapchainImageCount;

	INIT_STRUCT(VkDescriptorPoolCreateInfo, poolInfo);
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 4000;

	vkCreateDescriptorPool(g_VkDevice, &poolInfo, NULL, &g_DebugDescriptorPool);
}

void CreateSamplers()
{
	INIT_STRUCT(VkSamplerCreateInfo, samplerInfo);
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	vkCreateSampler(g_VkDevice, &samplerInfo, NULL, &g_DebugSampler);
}

void CreatePipelineLayout()
{
	// Ortho
	{
		VkPushConstantRange pushConstantRange;
		pushConstantRange.offset = 0;
		pushConstantRange.size = 6 * sizeof(float);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		INIT_STRUCT(VkPipelineLayoutCreateInfo, pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &g_DebugDescriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		if (vkCreatePipelineLayout(g_VkDevice, &pipelineLayoutInfo, NULL, &g_OrthoPipelineLayout) != VK_SUCCESS) {
			assert(0);
		}
	}

	// Persp
	{
		VkPushConstantRange pushConstantRange;
		pushConstantRange.offset = 0;
		pushConstantRange.size = 32 * sizeof(float);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		INIT_STRUCT(VkPipelineLayoutCreateInfo, pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &g_DebugDescriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		if (vkCreatePipelineLayout(g_VkDevice, &pipelineLayoutInfo, NULL, &g_PerspPipelineLayout) != VK_SUCCESS) {
			assert(0);
		}
	}
}

void CreateTestCommandBuffers()
{
	g_VkTestCommandBuffersCount = g_VkSwapchainImageCount;
	g_VkTestCommandBuffers = (VkCommandBuffer*)malloc(g_VkTestCommandBuffersCount * sizeof(VkCommandBuffer));

	INIT_STRUCT(VkCommandBufferAllocateInfo, allocInfo)
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = g_VkCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 3;

	if (vkAllocateCommandBuffers(g_VkDevice, &allocInfo, g_VkTestCommandBuffers) != VK_SUCCESS) {
		printf("Failed to allocate test command buffer!\n");
	}
}

void GenerateDebugDepthRTPSO(char* key)
{
	INIT_STRUCT(VkPipelineShaderStageCreateInfo, vertShaderStageInfo);
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = g_OrthoVertShaderModule;
	vertShaderStageInfo.pName = "main";

	INIT_STRUCT(VkPipelineShaderStageCreateInfo, fragShaderStageInfo);
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = g_DepthRTFragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescriptions[3];
	memset(&bindingDescriptions[0], 0, 3 * sizeof(VkVertexInputBindingDescription));
	// Vertices
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(vec4hack_t);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// Colors
	bindingDescriptions[1].binding = 1;
	bindingDescriptions[1].stride = sizeof(vec4hack_t);
	bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// UVs
	bindingDescriptions[2].binding = 2;
	bindingDescriptions[2].stride = sizeof(vec2hack_t);
	bindingDescriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescriptions[3];
	memset(&attributeDescriptions[0], 0, 3 * sizeof(VkVertexInputAttributeDescription));
	// Vertices
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[0].offset = 0;
	// Colors
	attributeDescriptions[1].binding = 1;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[1].offset = 0;
	// UVs
	attributeDescriptions[2].binding = 2;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = 0;

	INIT_STRUCT(VkPipelineVertexInputStateCreateInfo, vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 3;
	vertexInputInfo.vertexAttributeDescriptionCount = 3;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions[0];
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];

	INIT_STRUCT(VkPipelineInputAssemblyStateCreateInfo, inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	INIT_STRUCT(VkViewport, viewport);
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)g_VkSwapchainExtent.width;
	viewport.height = (float)g_VkSwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	INIT_STRUCT(VkRect2D, scissor);
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = g_VkSwapchainExtent;

	INIT_STRUCT(VkPipelineViewportStateCreateInfo, viewportState);
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	INIT_STRUCT(VkPipelineRasterizationStateCreateInfo, rasterizer);
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	INIT_STRUCT(VkPipelineMultisampleStateCreateInfo, multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	INIT_STRUCT(VkPipelineColorBlendAttachmentState, colorBlendAttachment);
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = 0;
	colorBlendAttachment.dstColorBlendFactor = 0;

	INIT_STRUCT(VkPipelineColorBlendStateCreateInfo, colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	INIT_STRUCT(VkPipelineDepthStencilStateCreateInfo, depthInfo);
	depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthInfo.depthTestEnable = VK_FALSE;

	INIT_STRUCT(VkGraphicsPipelineCreateInfo, pipelineInfo);
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &depthInfo;
	pipelineInfo.layout = g_OrthoPipelineLayout;
	pipelineInfo.renderPass = g_VkRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline* pso = (VkPipeline*)malloc(sizeof(VkPipeline));
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(g_VkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pso));

	map_set(&g_Q3ShaderMap, key, (void*)pso);
	OutputDebugString(key);
	OutputDebugString("\n");
}

void d_VK_InitVulkan()
{
	map_init(&g_Q3ShaderMap);
	memset(&g_BufferPool[0], 0, D_VK_BUFFER_POOL_SIZE * sizeof(VkBuffer));

	d_VKCore_InitApplication();

	CreateRenderPass();
	CreateFramebuffers();
	CreateCommandPool();

	CreateSamplers();

	CreateDebugDescriptorPool();
	CreateDebugDescriptorSetLayout();

	g_DrawDebugDepthRTDescriptorSet = CreateDrawDebugRTDescriptorSet(g_testDepth);

	CreateShaderModules();

	CreatePipelineLayout();

	CreateTestCommandBuffers();

	d_VKAllocation_Init(g_VkDevice, &g_VertexAttribsAllocation, 68435456, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	d_VKAllocation_Init(g_VkDevice, &g_IndexAllocation, 68435456, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	GenerateDebugDepthRTPSO("depthrt");
}


void DrawDebugRT(char* psoKey, VkImageView imageView, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	VkCommandBuffer commandBuffer = g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex]; // TODO: When I split 2D into another CB this needs to change!

	vec4hack_t verts[] = { {x,  y, 0.0, 0.0},
						   {x + w, y, 0.0, 0.0},
						   {x + w, y + h, 0.0, 0.0},
						   {x, y, 0.0, 0.0},
						   {x + w, y + h, 0.0, 0.0},
						   {x, y + h, 0.0, 0.0} };

	vec4hack_t colors[] = { {1.0, 1.0, 1.0, 1.0},
							{1.0, 1.0, 1.0, 1.0},
							{1.0, 1.0, 1.0, 1.0},
							{1.0, 1.0, 1.0, 1.0},
							{1.0, 1.0, 1.0, 1.0},
							{1.0, 1.0, 1.0, 1.0} };

	vec2hack_t uvs[] = { {0.0, 0.0},
						 {1.0, 0.0},
						 {1.0, 1.0},
						 {0.0, 0.0},
						 {1.0, 1.0},
						 {0.0, 1.0} };

	VkBuffer vertexBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, &verts[0], sizeof(vec4hack_t) * 6);
	VkBuffer colorBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, &colors[0], sizeof(vec4hack_t) * 6);
	VkBuffer uvsBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, &uvs[0], sizeof(vec2hack_t) * 6);

	VkBuffer vertexBuffers[] = { vertexBuffer, colorBuffer, uvsBuffer };
	VkDeviceSize offsets[] = { 0, 0, 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 3, &vertexBuffers[0], offsets);

	// Bind an appropriate PSO
	void* data = map_get(&g_Q3ShaderMap, psoKey);

	if (data != NULL) {
		VkPipeline** ppPSO = (VkPipeline * *)data;
		VkPipeline* pPSO = *ppPSO;
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pPSO);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_OrthoPipelineLayout, 0, 1, &g_DrawDebugDepthRTDescriptorSet, 0, NULL);

		float ortho[6] = { glConfig.vidWidth, 0.0, glConfig.vidHeight, 0.0, 1.0, -1.0 };
		vkCmdPushConstants(commandBuffer, g_OrthoPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 6 * sizeof(float), &ortho[0]);


		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}
}


size_t g_3Ddraws = 0;
void d_VK_BeginFrame()
{
	vkAcquireNextImageKHR(g_VkDevice, g_VkSwapchain, (uint64_t)(-1), g_ImageAvailableSemaphore, VK_NULL_HANDLE, &g_CurrentSwapchainImageIndex);

	g_3Ddraws = 0;

	g_VertexAttribsAllocation.currentBufferOffset = 0;
	g_IndexAllocation.currentBufferOffset = 0;

	{
		// Begin command buffer recording for this swapchain image index
		INIT_STRUCT(VkCommandBufferBeginInfo, beginInfo);
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], &beginInfo) != VK_SUCCESS) {
			printf("Failed to record command buffer!\n");
		}

		INIT_STRUCT(VkRenderPassBeginInfo, renderPassInfo);
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = g_VkRenderPass;
		renderPassInfo.framebuffer = g_VkFramebuffers[g_CurrentSwapchainImageIndex];
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = g_VkSwapchainExtent;

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkClearValue depthStencil = { 1.0f, 0 };
		VkClearValue clearValues[] = { clearColor, depthStencil };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = &clearValues[0];

		vkCmdBeginRenderPass(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}
}

void d_VK_EndFrame()
{
	// Draw debug stuff if any.
	// DrawDebugRT("depthrt", g_testDepth, 10, 10, 400, 300);

	// End recording of 2D command buffer
	vkCmdEndRenderPass(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex]);
	VK_CHECK_RESULT(vkEndCommandBuffer(g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex]));

	// Submit command buffer
	INIT_STRUCT(VkSubmitInfo, submitInfo)
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { g_ImageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	VkCommandBuffer commandBuffers[] = { g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex] };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[0];

	VkSemaphore signalSemaphores[] = { g_RenderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(g_VkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		printf("Failed to submit command!\n");
	}

	VkPresentInfoKHR presentInfo;
	memset(&presentInfo, 0, sizeof(VkPresentInfoKHR));
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { g_VkSwapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &g_CurrentSwapchainImageIndex;

	vkQueuePresentKHR(g_VkPresentQueue, &presentInfo);

	vkQueueWaitIdle(g_VkPresentQueue);
}

void d_VK_CreateImage(unsigned* pixels,
	int32_t width, 
	int32_t height,
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
	VkDescriptorSet* vkDescriptorSet)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	VkMemoryRequirements stagingReqs;
	size_t textureSize = width * height * 4;
	
	// Create a host-visible staging buffer that contains the raw image data
	INIT_STRUCT(VkBufferCreateInfo, bufferCI);
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.size = textureSize;
	bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	d_VKUtils_CreateBuffer(&g_VkDevice, &bufferCI, &stagingBuffer, &stagingMemory, &stagingReqs);

	void* data;
	vkMapMemory(g_VkDevice, stagingMemory, 0, textureSize, 0, &data);
	memcpy(data, pixels, textureSize);
	vkUnmapMemory(g_VkDevice, stagingMemory);

	d_VKUtils_CreateImage(&g_VkDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vkImage, vkDeviceMemory);

	VkCommandBuffer copyCmd = d_VKUtils_CreateCommandBuffer(&g_VkDevice, &g_VkCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_TRUE);

	INIT_STRUCT(VkImageSubresourceRange, subresourceRange);
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	INIT_STRUCT(VkImageMemoryBarrier, imageMemoryBarrier);
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = *vkImage;
	imageMemoryBarrier.subresourceRange = subresourceRange;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	vkCmdPipelineBarrier(
		copyCmd,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);

	INIT_STRUCT(VkBufferImageCopy, bufferCopyRegion);
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = width;
	bufferCopyRegion.imageExtent.height = height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		*vkImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion);

	// Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition 
	// Source pipeline stage stage is copy command exection (VK_PIPELINE_STAGE_TRANSFER_BIT)
	// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
	vkCmdPipelineBarrier(
		copyCmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);

	d_VKUtils_FlushCommandBuffer(g_VkDevice, g_VkCommandPool, copyCmd, g_VkGraphicsQueue, VK_TRUE);

	// Clean up staging resources
	vkFreeMemory(g_VkDevice, stagingMemory, NULL);
	vkDestroyBuffer(g_VkDevice, stagingBuffer, NULL);

	// Create ImageView
	INIT_STRUCT(VkImageViewCreateInfo, viewInfo);
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *vkImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	vkCreateImageView(g_VkDevice, &viewInfo, NULL, vkImageView);

	// Create Descriptor Set for this image,
	// DONOTCOMMIT: Is there a better way?
	{
		VkDescriptorSetLayout layout[] = { g_DebugDescriptorSetLayout };
		INIT_STRUCT(VkDescriptorSetAllocateInfo, allocInfo);
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = g_DebugDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout[0];
		vkAllocateDescriptorSets(g_VkDevice, &allocInfo, vkDescriptorSet);

		INIT_STRUCT(VkDescriptorImageInfo, imageInfo);
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = *vkImageView;
		imageInfo.sampler = g_DebugSampler;

		INIT_STRUCT(VkWriteDescriptorSet, descWrite);
		descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrite.dstSet = *vkDescriptorSet;
		descWrite.dstBinding = 0;
		descWrite.dstArrayElement = 0;
		descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descWrite.descriptorCount = 1;
		descWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(g_VkDevice, 1, &descWrite, 0, NULL);
	}
}

void d_VK_DestroyImage(image_t* image)
{
	if (image->vkImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(g_VkDevice, image->vkImageView, NULL);
		image->vkImageView = VK_NULL_HANDLE;
	}

	if (image->vkImage != VK_NULL_HANDLE) {
		vkDestroyImage(g_VkDevice, image->vkImage, NULL);
		image->vkImage = VK_NULL_HANDLE;
	}

	// TODO Free DeviceMemory and DescriptorSets
}

VkBlendFactor Q3ToVkSrcBlendFactor(uint32_t s)
{
	int blendSrcBits = s & GLS_SRCBLEND_BITS;

	switch (blendSrcBits) {
	case GLS_SRCBLEND_ZERO:
		return VK_BLEND_FACTOR_ZERO;
	case GLS_SRCBLEND_ONE:
		return VK_BLEND_FACTOR_ONE;
	case GLS_SRCBLEND_DST_COLOR:
		return VK_BLEND_FACTOR_DST_COLOR;
	case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case GLS_SRCBLEND_SRC_ALPHA:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case GLS_SRCBLEND_DST_ALPHA:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case GLS_SRCBLEND_ALPHA_SATURATE:
		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	}
	
	return VK_BLEND_FACTOR_MAX_ENUM;
}

VkBlendFactor Q3ToVkDstBlendFactor(uint32_t d)
{
	int blendDstBits = d & GLS_DSTBLEND_BITS;

	switch (blendDstBits) {
	case GLS_DSTBLEND_ZERO:
		return VK_BLEND_FACTOR_ZERO;
	case GLS_DSTBLEND_ONE:
		return VK_BLEND_FACTOR_ONE;
	case GLS_DSTBLEND_SRC_COLOR:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case GLS_DSTBLEND_SRC_ALPHA:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case GLS_DSTBLEND_DST_ALPHA:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	}

	return VK_BLEND_FACTOR_MAX_ENUM;
}

void GeneratePSOFromShaderStage(char* key, shader_t* shader, size_t stage)
{
	INIT_STRUCT(VkPipelineShaderStageCreateInfo, vertShaderStageInfo);
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = backEnd.projection2D == qtrue ? g_OrthoVertShaderModule : g_PerspVertShaderModule;
	vertShaderStageInfo.pName = "main";

	INIT_STRUCT(VkPipelineShaderStageCreateInfo, fragShaderStageInfo);
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = g_OrthoFragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescriptions[3];
	memset(&bindingDescriptions[0], 0, 3 * sizeof(VkVertexInputBindingDescription));
	// Vertices
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(vec4hack_t);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// Colors
	bindingDescriptions[1].binding = 1;
	bindingDescriptions[1].stride = sizeof(vec4hack_t);
	bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// UVs
	bindingDescriptions[2].binding = 2;
	bindingDescriptions[2].stride = sizeof(vec2hack_t);
	bindingDescriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescriptions[3];
	memset(&attributeDescriptions[0], 0, 3 * sizeof(VkVertexInputAttributeDescription));
	// Vertices
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[0].offset = 0;
	// Colors
	attributeDescriptions[1].binding = 1;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[1].offset = 0;
	// UVs
	attributeDescriptions[2].binding = 2;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = 0;

	INIT_STRUCT(VkPipelineVertexInputStateCreateInfo, vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 3;
	vertexInputInfo.vertexAttributeDescriptionCount = 3;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions[0];
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];

	INIT_STRUCT(VkPipelineInputAssemblyStateCreateInfo, inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	INIT_STRUCT(VkViewport, viewport);
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)g_VkSwapchainExtent.width;
	viewport.height = (float)g_VkSwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	INIT_STRUCT(VkRect2D, scissor);
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = g_VkSwapchainExtent;

	INIT_STRUCT(VkPipelineViewportStateCreateInfo, viewportState);
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	INIT_STRUCT(VkPipelineRasterizationStateCreateInfo, rasterizer);
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	/*if (glState.faceCulling == CT_TWO_SIDED) {
		rasterizer.cullMode = VK_CULL_MODE_NONE;
	}
	else {
		if (glState.faceCulling == CT_BACK_SIDED) {
			if (backEnd.viewParms.isMirror) {
				rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			}
			else {
				rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			}
		}
		else {
			if (backEnd.viewParms.isMirror) {
				rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			}
			else {
				rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
			}
		}
	}*/

	INIT_STRUCT(VkPipelineMultisampleStateCreateInfo, multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	INIT_STRUCT(VkPipelineColorBlendAttachmentState, colorBlendAttachment);
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.srcColorBlendFactor = Q3ToVkSrcBlendFactor(shader->stages[stage]->stateBits);
	colorBlendAttachment.dstColorBlendFactor = Q3ToVkDstBlendFactor(shader->stages[stage]->stateBits);
	colorBlendAttachment.blendEnable = colorBlendAttachment.srcColorBlendFactor != VK_BLEND_FACTOR_MAX_ENUM && colorBlendAttachment.dstColorBlendFactor != VK_BLEND_FACTOR_MAX_ENUM;
	if (colorBlendAttachment.blendEnable == VK_FALSE) {
		colorBlendAttachment.srcColorBlendFactor = 0;
		colorBlendAttachment.dstColorBlendFactor = 0;
	}
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

	INIT_STRUCT(VkPipelineColorBlendStateCreateInfo, colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	INIT_STRUCT(VkPipelineDynamicStateCreateInfo, dynamicInfo);
	dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicInfo.dynamicStateCount = 1;
	VkDynamicState ds[] = { VK_DYNAMIC_STATE_VIEWPORT };
	dynamicInfo.pDynamicStates = &ds[0];

	INIT_STRUCT(VkPipelineDepthStencilStateCreateInfo, depthStencilCreateInfo);
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;// !backEnd.projection2D;
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;// !backEnd.projection2D;
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS; // VK_COMPARE_OP_LESS;
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	INIT_STRUCT(VkGraphicsPipelineCreateInfo, pipelineInfo);
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = &shaderStages[0];
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = backEnd.projection2D ? NULL : &dynamicInfo;
	pipelineInfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineInfo.layout = backEnd.projection2D ? g_OrthoPipelineLayout : g_PerspPipelineLayout;
	pipelineInfo.renderPass = g_VkRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline* pso = (VkPipeline*)malloc(sizeof(VkPipeline));
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(g_VkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pso));

	map_set(&g_Q3ShaderMap, key, (void*)pso);
	OutputDebugString(key);
	OutputDebugString("\n");
}

glm_vkperspective(float fovy,
	float aspect,
	float nearVal,
	float farVal,
	mat4  dest) {
	float f, fn;

	glm_mat4_zero(dest);

	f = 1.0f / tanf(glm_rad(fovy * 0.5f));
	fn = 1.0f / (nearVal - farVal);

	dest[0][0] = f / aspect;
	dest[1][1] = -f;
	dest[2][2] = farVal / (nearVal - farVal);
	dest[2][3] = -1.0f;
	dest[3][2] = (nearVal * farVal) / (nearVal - farVal);
}

void d_VK_DrawTris(shaderCommands_t* input, uint32_t stage, VkBool32 useTexCoords0)
{
	//if (strcmp(input->shader->name, "models/players/hud/allied_soldier") != 0)
	//	return;
	/*if (!backEnd.projection2D && g_3Ddraws > 30)
		return;*/

	// Pick what command buffer this call will go into, for now just 2D/3D. Eventually we'll split 3D Scene, Arm/Weapon, Avatar health indicator.
	VkCommandBuffer commandBuffer = g_VkTestCommandBuffers[g_CurrentSwapchainImageIndex];

	VkBuffer vertexBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, input->xyz, sizeof(vec4hack_t)*input->numVertexes);
	VkBuffer indexBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_IndexAllocation, input->indexes, sizeof(int) * input->numIndexes);
	VkBuffer uvsBuffer = VK_NULL_HANDLE;
	if (useTexCoords0)
		uvsBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, input->texCoords0, sizeof(vec2hack_t) * input->numVertexes);
	else
		uvsBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, &input->svars.texcoords[0], sizeof(vec2hack_t) * input->numVertexes);

	// TODO: THIS WAS A TEST, NOT NEEDED, REVERT!
	vec4hack_t* colors = (vec4hack_t*)malloc(input->numVertexes * sizeof(vec4hack_t));
	for (int i = 0; i < input->numVertexes; ++i) {
		for (int j = 0; j < 4; ++j) {
			colors[i].v[j] = input->svars.colors[i][j] / 255.0;
		}
	}
	VkBuffer colorBuffer = d_VKAllocation_AcquireChunk(g_VkDevice, &g_VertexAttribsAllocation, colors, sizeof(vec4hack_t) * input->numVertexes);
	free(colors);

	VkBuffer vertexBuffers[] = { vertexBuffer, colorBuffer, uvsBuffer };
	VkDeviceSize offsets[] = { 0, 0, 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 3, &vertexBuffers[0], offsets);

	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	{
		// Bind an appropriate PSO

		char key[512];
		sprintf(&key[0], "%s:p[%d]:%d:%d", input->shader->name, stage, backEnd.projection2D, input->shader->cullType);
		void* data = map_get(&g_Q3ShaderMap, key);

		if (data == NULL) {
			// Generate PSO for this shader

			// Hardcode generating PSO from shader stage 0
			GeneratePSOFromShaderStage(key, input->shader, stage);

			data = map_get(&g_Q3ShaderMap, key);
		}

		VkPipeline** ppPSO = (VkPipeline * *)data;
		VkPipeline* pPSO = *ppPSO;
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pPSO);
	}

	if (backEnd.projection2D == qtrue) {
		{
			{
				image_t* img = img = input->shader->stages[stage]->bundle[0].image[0];

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_OrthoPipelineLayout, 0, 1, &img->vkDescriptorSet, 0, NULL);
			}
		}

		float ortho[6] = { glConfig.vidWidth, 0.0, glConfig.vidHeight, 0.0, 1.0, -1.0 };
		vkCmdPushConstants(commandBuffer, g_OrthoPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 6 * sizeof(float), &ortho[0]);
	}
	else {
		g_3Ddraws++;
		{
			{
				image_t* img = img = input->shader->stages[stage]->bundle[0].image[0];

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_PerspPipelineLayout, 0, 1, &img->vkDescriptorSet, 0, NULL);
			}
		}

		//mat4 persp;
		//glm_vkperspective(backEnd.viewParms.fovY, backEnd.viewParms.viewportWidth/(float)backEnd.viewParms.viewportHeight, 1, 100, &persp);

		float mats[32];
		//memcpy(&mats[0], &persp[0], 16 * sizeof(float));
		memcpy(&mats[0], &backEnd.viewParms.projectionMatrix[0], 16 * sizeof(float));
		memcpy(&mats[16], &backEnd.orientation.modelMatrix[0], 16 * sizeof(float));
		vkCmdPushConstants(commandBuffer, g_PerspPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 32 * sizeof(float), &mats[0]);

		INIT_STRUCT(VkViewport, vp);
		vp.x = backEnd.viewParms.viewportX;
		vp.y = g_VkSwapchainExtent.height - backEnd.viewParms.viewportY - backEnd.viewParms.viewportHeight; // Viewport x,y in GL is lower left corner as opposed to upper left
		vp.width = backEnd.viewParms.viewportWidth;
		vp.height = backEnd.viewParms.viewportHeight;
		vkCmdSetViewport(commandBuffer, 0, 1, &vp);
	}

	vkCmdDrawIndexed(commandBuffer, input->numIndexes, 1, 0, 0, 0);

	// Queue up buffers to be freed and free old ones in round robin way
	// TODO: Better, and/or threaded
	VkBuffer freeBuffers[] = { vertexBuffer, colorBuffer, uvsBuffer, indexBuffer };
	for (int i = 0; i < 4; ++i) {
		if (g_BufferPool[g_CurrentBufferPoolIndex] != VK_NULL_HANDLE) {
			vkDestroyBuffer(g_VkDevice, g_BufferPool[g_CurrentBufferPoolIndex], NULL);
		}

		g_BufferPool[g_CurrentBufferPoolIndex] = freeBuffers[i];
		g_CurrentBufferPoolIndex = (g_CurrentBufferPoolIndex + 1) % D_VK_BUFFER_POOL_SIZE;
	}
}
