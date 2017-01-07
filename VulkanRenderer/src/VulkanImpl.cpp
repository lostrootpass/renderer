#include "VulkanImpl.h"
#include "VulkanImplUtil.h"
#include "SwapChain.h"
#include "ShaderCache.h"
#include "Model.h"
#include "Camera.h"
#include "Scene.h"

#include <set>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

VkDevice VulkanImpl::_device = VK_NULL_HANDLE;
VkPhysicalDevice VulkanImpl::_physicalDevice = VK_NULL_HANDLE;

typedef enum SetBindings
{
	SET_BINDING_CAMERA,
	SET_BINDING_MODEL,
	SET_BINDING_SAMPLER,
	SET_BINDING_COUNT //Always have this be last.
} SetBindings;

VulkanImpl::VulkanImpl() : _swapChain(nullptr)
{

}

VulkanImpl::~VulkanImpl()
{
	_cleanup();
}

void VulkanImpl::copyBuffer(const Buffer& dst, const Buffer& src, VkDeviceSize size) const
{
	VkCommandBuffer buffer = startOneShotCmdBuffer();

	VkBufferCopy copy = {};
	copy.size = size;

	vkCmdCopyBuffer(buffer, src.buffer, dst.buffer, 1, &copy);
	
	submitOneShotCmdBuffer(buffer);
}

void VulkanImpl::createAndBindBuffer(const VkBufferCreateInfo& info, Buffer& buffer, VkMemoryPropertyFlags flags) const
{
	VkMemoryRequirements memReq;

	vkCreateBuffer(_device, &info, nullptr, &buffer.buffer);
	vkGetBufferMemoryRequirements(VulkanImpl::device(), buffer.buffer, &memReq);

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = getMemoryTypeIndex(memReq.memoryTypeBits, flags);

	vkAllocateMemory(VulkanImpl::device(), &alloc, nullptr, &buffer.memory);

	vkBindBufferMemory(VulkanImpl::device(), buffer.buffer, buffer.memory, 0);
}

Uniform* VulkanImpl::createUniform(const std::string& name, size_t size)
{
	if (_uniforms.find(name) != _uniforms.end())
	{
		//TODO: delete uniform then continue to recreate it?
		return _uniforms[name];
	}

	Uniform* uniform = new Uniform;
	_uniforms[name] = uniform;
	
	uniform->size = size;

	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.size = size;

	createAndBindBuffer(info, uniform->stagingBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	createAndBindBuffer(info, uniform->localBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	return uniform;
}

uint32_t VulkanImpl::getMemoryTypeIndex(uint32_t bits, VkMemoryPropertyFlags flags) const
{
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &props);

	for (uint32_t i = 0; i < props.memoryTypeCount; i++)
	{
		if (((props.memoryTypes[i].propertyFlags & flags) == flags) && (bits & (1 << i)))
		{
			return i;
		}
	}

	return -1;
}

const VkPipeline VulkanImpl::getPipelineForShader(const std::string& shaderName)
{
	if (_pipelines.find(shaderName) != _pipelines.end())
		return _pipelines[shaderName];

	VkPipeline pipeline;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[0].module = ShaderCache::getModule(shaderName + ".vert");

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";
	stages[1].module = ShaderCache::getModule(shaderName + ".frag");

	VkPipelineColorBlendAttachmentState cba = {};
	cba.blendEnable = VK_FALSE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo cbs = {};
	cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cba;
	cbs.logicOp = VK_LOGIC_OP_COPY;

	VkPipelineInputAssemblyStateCreateInfo ias = {};
	ias.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	ias.primitiveRestartEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo mss = {};
	mss.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	mss.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.lineWidth = 1.0f;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkVertexInputBindingDescription vbs = {};
	vbs.binding = 0;
	vbs.stride = sizeof(Vertex);
	vbs.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vtxAttrs[3] = {};
	vtxAttrs[0].binding = 0;
	vtxAttrs[0].location = 0;
	vtxAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vtxAttrs[0].offset = offsetof(Vertex, position);

	vtxAttrs[1].binding = 0;
	vtxAttrs[1].location = 1;
	vtxAttrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vtxAttrs[1].offset = offsetof(Vertex, color);

	vtxAttrs[2].binding = 0;
	vtxAttrs[2].location = 2;
	vtxAttrs[2].format = VK_FORMAT_R32G32_SFLOAT;
	vtxAttrs[2].offset = offsetof(Vertex, uv);

	VkPipelineVertexInputStateCreateInfo vis = {};
	vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vis.vertexBindingDescriptionCount = 1;
	vis.pVertexBindingDescriptions = &vbs;
	vis.vertexAttributeDescriptionCount = 3;
	vis.pVertexAttributeDescriptions = vtxAttrs;

	VkExtent2D extent = _swapChain->surfaceCapabilities().currentExtent;
	VkRect2D sc = { 0, 0, extent.width, extent.height };

	VkViewport vp = {};
	vp.width = (float)extent.width;
	vp.height = (float)extent.height;
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;

	VkPipelineViewportStateCreateInfo vps = {};
	vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vps.viewportCount = 1;
	vps.scissorCount = 1;
	vps.pViewports = &vp;
	vps.pScissors = &sc;

	VkPipelineDepthStencilStateCreateInfo dss = {};
	dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dss.depthTestEnable = VK_TRUE;
	dss.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	dss.depthWriteEnable = VK_TRUE;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.layout = _pipelineLayout;
	info.renderPass = _renderPass;
	info.stageCount = 2;
	info.pStages = stages;
	info.pColorBlendState = &cbs;
	info.pInputAssemblyState = &ias;
	info.pMultisampleState = &mss;
	info.pRasterizationState = &rs;
	info.pVertexInputState = &vis;
	info.pViewportState = &vps;
	info.pDepthStencilState = &dss;

	if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
	{
		//
	}

	_pipelines[shaderName] = pipeline;
	return _pipelines[shaderName];
}

void VulkanImpl::init(const Window& window)
{
	_createInstance();

	//Register the debugger at the earliest opportunity, which is immediately after we have a VkInstance
	_registerDebugger();


	window.createSurface(_instance, &_surface);
	_initDevice();
	_createCommandPool();
	ShaderCache::init();
	_createRenderPass();
	_createSwapChain();
	_createLayouts();
	_createSampler();
	_createDescriptorSets();
	_allocateCommandBuffers();
}

void VulkanImpl::recordCommandBuffers(const Scene* scene)
{
	const std::vector<VkFramebuffer> framebuffers = _swapChain->framebuffers();

	VkClearValue clearValues[] = {
		{ 0.0f, 0.0f, 0.0f, 1.0f }, //Clear color
		{ 1.0f, 0.0f } //Depth stencil
	};

	for (size_t i = 0; i < _commandBuffers.size(); i++)
	{
		VkCommandBuffer buffer = _commandBuffers[i];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.clearValueCount = 2;
		info.pClearValues = clearValues;
		info.renderPass = _renderPass;
		info.renderArea.offset = { 0, 0 };
		info.renderArea.extent = _swapChain->surfaceCapabilities().currentExtent;
		info.framebuffer = framebuffers[i];

		vkBeginCommandBuffer(buffer, &beginInfo);

		vkCmdBeginRenderPass(buffer, &info, VK_SUBPASS_CONTENTS_INLINE);

		//vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, SET_BINDING_COUNT, _descriptorSets.data(), 0, nullptr);

		if(scene)
			scene->draw(buffer);

		vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, SET_BINDING_COUNT, _descriptorSets.data(), 0, nullptr);

		vkCmdEndRenderPass(buffer);

		vkEndCommandBuffer(buffer);
	}
}

void VulkanImpl::render()
{
	_swapChain->present();
}

void VulkanImpl::setImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange& range) const
{
	VkCommandBuffer buffer = startOneShotCmdBuffer();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.image = image;
	barrier.subresourceRange = range;

	switch (oldLayout)
	{
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	}

	switch (newLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	}

	vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);	

	submitOneShotCmdBuffer(buffer);
}

VkCommandBuffer VulkanImpl::startOneShotCmdBuffer() const
{
	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = _commandPool;
	info.commandBufferCount = 1;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer buffer;
	vkAllocateCommandBuffers(_device, &info, &buffer);

	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(buffer, &begin);

	return buffer;
}

void VulkanImpl::submitOneShotCmdBuffer(VkCommandBuffer buffer) const
{
	vkEndCommandBuffer(buffer);

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &buffer;

	vkQueueSubmit(_graphicsQueue.vkQueue, 1, &submit, VK_NULL_HANDLE);
	vkQueueWaitIdle(_graphicsQueue.vkQueue);

	vkFreeCommandBuffers(_device, _commandPool, 1, &buffer);
}

void VulkanImpl::updateSampledImage(VkImageView view) const
{
	VkDescriptorImageInfo info = {};
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.imageView = view;

	VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.dstSet = _descriptorSets[SET_BINDING_MODEL];
	write.dstBinding = 1;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
}

void VulkanImpl::updateUniform(const std::string& name, void* data, size_t size)
{
	if (_uniforms.find(name) == _uniforms.end())
		return;

	Uniform* uniform = _uniforms[name];

	void* dst;
	vkMapMemory(_device, uniform->stagingBuffer.memory, 0, size, 0, &dst);
	memcpy(dst, data, size);
	vkUnmapMemory(_device, uniform->stagingBuffer.memory);

	copyBuffer(uniform->localBuffer, uniform->stagingBuffer, size);
}

void VulkanImpl::_allocateCommandBuffers()
{
	const std::vector<VkFramebuffer> framebuffers = _swapChain->framebuffers();
	_commandBuffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandBufferCount = (uint32_t)_commandBuffers.size();
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandPool = _commandPool;

	if (vkAllocateCommandBuffers(_device, &info, _commandBuffers.data()) != VK_SUCCESS)
	{
		//
	}

	recordCommandBuffers();
}

void VulkanImpl::_cleanup()
{
	vkDeviceWaitIdle(_device);

	for (auto pair : _uniforms)
	{
		delete pair.second;
	}

	_uniforms.clear();

	vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);

	vkDestroySampler(_device, _sampler, nullptr);
	ShaderCache::shutdown();

	for (auto pair : _pipelines)
	{
		vkDestroyPipeline(_device, pair.second, nullptr);
	}

	_pipelines.clear();

	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

	for (VkDescriptorSetLayout layout : _descriptorLayouts)
	{
		vkDestroyDescriptorSetLayout(_device, layout, nullptr);
	}

	delete _swapChain;
	_swapChain = nullptr;

	vkDestroyRenderPass(_device, _renderPass, nullptr);
	vkDestroyCommandPool(_device, _commandPool, nullptr);
	vkDestroyDevice(_device, nullptr);
	_device = VK_NULL_HANDLE;
	vkDestroySurfaceKHR(_instance, _surface, nullptr);

	if (DEBUGENABLE)
	{
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
			(PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
		if (vkDestroyDebugReportCallbackEXT)
			vkDestroyDebugReportCallbackEXT(_instance, _debugCallback, nullptr);
	}

	vkDestroyInstance(_instance, nullptr);
}

void VulkanImpl::_createCommandPool()
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = _graphicsQueue.index;

	if (vkCreateCommandPool(_device, &info, nullptr, &_commandPool) != VK_SUCCESS)
	{
		//
	}
}

void VulkanImpl::_createDescriptorSets()
{
	VkDescriptorPoolSize sizes[SET_BINDING_COUNT] = {};
	sizes[0].descriptorCount = 2;
	sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	sizes[1].descriptorCount = 1;
	sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;

	sizes[2].descriptorCount = 1;
	sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

	VkDescriptorPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool.poolSizeCount = SET_BINDING_COUNT;
	pool.pPoolSizes = sizes;
	pool.maxSets = SET_BINDING_COUNT;

	if (vkCreateDescriptorPool(_device, &pool, nullptr, &_descriptorPool) != VK_SUCCESS)
	{
		//
	}

	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = _descriptorPool;
	alloc.pSetLayouts = _descriptorLayouts.data();
	alloc.descriptorSetCount = (uint32_t)_descriptorLayouts.size();

	_descriptorSets.resize(SET_BINDING_COUNT);
	
	if (vkAllocateDescriptorSets(_device, &alloc, _descriptorSets.data()) != VK_SUCCESS)
	{
		//
	}

	{
		VkDescriptorBufferInfo buff = {};
		Uniform* uniform = createUniform("camera", sizeof(glm::mat4));
		buff.buffer = uniform->localBuffer.buffer;
		buff.offset = 0;
		buff.range = uniform->size;

		VkWriteDescriptorSet writes[1] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstSet = _descriptorSets[SET_BINDING_CAMERA];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pBufferInfo = &buff;

		vkUpdateDescriptorSets(_device, 1, writes, 0, nullptr);
	}

	{

		VkDescriptorBufferInfo buff = {};
		Uniform* uniform = createUniform("model", sizeof(glm::mat4));
		buff.buffer = uniform->localBuffer.buffer;
		buff.offset = 0;
		buff.range = uniform->size;

		VkWriteDescriptorSet writes[2] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstSet = _descriptorSets[SET_BINDING_MODEL];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pBufferInfo = &buff;

		vkUpdateDescriptorSets(_device, 1, writes, 0, nullptr);
	}

	{
		VkDescriptorImageInfo img = {};
		img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img.sampler = _sampler;

		VkWriteDescriptorSet writes[1] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writes[0].dstSet = _descriptorSets[SET_BINDING_SAMPLER];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pImageInfo = &img;

		vkUpdateDescriptorSets(_device, 1, writes, 0, nullptr);
	}
}

void VulkanImpl::_createInstance()
{
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.apiVersion = VK_API_VERSION_1_0;
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pApplicationName = "Vulkan Renderer";
	applicationInfo.pEngineName = "No Engine";
	applicationInfo.pNext = nullptr;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &applicationInfo;

	std::vector<const char*> extensions;
	Util::getRequiredExtensions(extensions);
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (DEBUGENABLE)
	{
		const char* validationLayerNames[] = {
			"VK_LAYER_LUNARG_standard_validation"
		};

		createInfo.enabledLayerCount = 1;
		createInfo.ppEnabledLayerNames = validationLayerNames;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}
	
	if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS)
	{
		//
	}
}

void VulkanImpl::_createLayouts()
{
	VkDescriptorSetLayoutBinding bindings[2] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].descriptorCount = 1;

	bindings[1].binding = 1;
	bindings[1].descriptorCount = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 1;
	info.pBindings = bindings;

	VkDescriptorSetLayout layouts[SET_BINDING_COUNT];

	if (vkCreateDescriptorSetLayout(_device, &info, nullptr, &layouts[0]) != VK_SUCCESS)
	{
		//
	}

	info.bindingCount = 2;
	vkCreateDescriptorSetLayout(_device, &info, nullptr, &layouts[1]);

	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].descriptorCount = 1;
	vkCreateDescriptorSetLayout(_device, &info, nullptr, &layouts[2]);

	_descriptorLayouts.push_back(layouts[0]);
	_descriptorLayouts.push_back(layouts[1]);
	_descriptorLayouts.push_back(layouts[2]);

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = _descriptorLayouts.data();
	layoutCreateInfo.setLayoutCount = (uint32_t)_descriptorLayouts.size();

	if (vkCreatePipelineLayout(_device, &layoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS)
	{
		//
	}
}

void VulkanImpl::_createRenderPass()
{
	//Color
	VkAttachmentDescription attachDesc = {};
	attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkAttachmentReference attachRef = {};
	attachRef.attachment = 0;
	attachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachRef;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;


	//Depth
	VkAttachmentReference depthAttach = {};
	depthAttach.attachment = 1;
	depthAttach.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription depthPass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthAttach;

	VkAttachmentDescription depthDesc = {};
	depthDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	depthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthDesc.format = VK_FORMAT_D32_SFLOAT;


	VkSubpassDependency dependency = {};
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;

	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstSubpass = 0;

	VkAttachmentDescription attachments[] = { attachDesc, depthDesc };
	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 2;
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;

	if (vkCreateRenderPass(_device, &info, nullptr, &_renderPass) != VK_SUCCESS)
	{
		//	
	}
}

void VulkanImpl::_createSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;

	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;

	if (vkCreateSampler(_device, &samplerInfo, nullptr, &_sampler) != VK_SUCCESS)
	{
		//
	}
}

void VulkanImpl::_createSwapChain()
{
	uint32_t indices[] = { _graphicsQueue.index, _presentQueue.index };
	_swapChain = new SwapChain(*this);
	VkSurfaceCapabilitiesKHR caps = _swapChain->surfaceCapabilities();

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.clipped = VK_TRUE;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	info.surface = _surface;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.minImageCount = caps.minImageCount;
	info.imageExtent.width = caps.currentExtent.width;
	info.imageExtent.height = caps.currentExtent.height;
	info.preTransform = caps.currentTransform;
	info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

	if (indices[0] == indices[1])
	{
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	else
	{
		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		info.queueFamilyIndexCount = 2;
		info.pQueueFamilyIndices = indices;
	}

	_swapChain->init(info);
	_extent = _swapChain->surfaceCapabilities().currentExtent;
}

void VulkanImpl::_initDevice()
{
	_physicalDevice = _pickPhysicalDevice();

	VkPhysicalDeviceFeatures features = {};

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	std::vector<const char*> extensions = { 
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.pEnabledFeatures = &features;
	info.ppEnabledExtensionNames = extensions.data();
	info.enabledExtensionCount = (uint32_t)extensions.size();

	const float priority = 1.0f;
	_queryDeviceQueueFamilies(_physicalDevice);
	
	std::vector<VkDeviceQueueCreateInfo> queryInfos;
	std::set<uint32_t> uniqueFamilies = { _graphicsQueue.index, _presentQueue.index };

	for (uint32_t family : uniqueFamilies)
	{
		VkDeviceQueueCreateInfo dqInfo = {};
		dqInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		dqInfo.queueCount = 1;
		dqInfo.queueFamilyIndex = family;
		dqInfo.pQueuePriorities = &priority;
		queryInfos.push_back(dqInfo);
	}

	info.queueCreateInfoCount = (uint32_t)queryInfos.size();
	info.pQueueCreateInfos = queryInfos.data();

	if (DEBUGENABLE)
	{
		info.enabledLayerCount = (uint32_t)ARRAYSIZE(VK_VALIDATION_LAYERS);
		info.ppEnabledLayerNames = VK_VALIDATION_LAYERS;
	}

	if (vkCreateDevice(_physicalDevice, &info, nullptr, &_device) != VK_SUCCESS)
	{
		//
	}

	vkGetDeviceQueue(_device, _graphicsQueue.index, 0, &_graphicsQueue.vkQueue);
	vkGetDeviceQueue(_device, _presentQueue.index, 0, &_presentQueue.vkQueue);
}

VkPhysicalDevice VulkanImpl::_pickPhysicalDevice()
{
	uint32_t physicalDeviceCount;
	vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	if (physicalDeviceCount > 0)
	{
		VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[physicalDeviceCount];
		vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices);
		physicalDevice = physicalDevices[0]; //default to any GPU in case it's all we have.


		//Pick the first discrete GPU we encounter.
		for (uint32_t i = 0; i < physicalDeviceCount; i++)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = physicalDevices[i];
				break;
			}
		}

		delete[] physicalDevices;
	}

	return physicalDevice;
}

void VulkanImpl::_queryDeviceQueueFamilies(VkPhysicalDevice device)
{
	_graphicsQueue.index = -1, _presentQueue.index = -1;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, families.data());

	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		if (families[i].queueCount > 0 && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			 _graphicsQueue.index = i;
		}

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);

		if (families[i].queueCount > 0 && presentSupport) {
			_presentQueue.index = i;
		}

		if (_graphicsQueue.index != -1 && _presentQueue.index != -1)
			break;
	}
}

void VulkanImpl::_registerDebugger()
{
	if (!DEBUGENABLE) return;

	VkDebugReportCallbackCreateInfoEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	info.pfnCallback = Util::debugCallback;
	info.pUserData = (void*)this;
	info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = 
		(PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugReportCallbackEXT");
	if(vkCreateDebugReportCallbackEXT)
		vkCreateDebugReportCallbackEXT(_instance, &info, nullptr, &_debugCallback);
}