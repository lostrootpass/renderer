#include "VulkanImpl.h"
#include "VulkanImplUtil.h"
#include "SwapChain.h"
#include "ShaderCache.h"
#include "Model.h"

#include <set>

VkDevice VulkanImpl::_device = VK_NULL_HANDLE;
VkPhysicalDevice VulkanImpl::_physicalDevice = VK_NULL_HANDLE;

VulkanImpl::VulkanImpl() : _swapChain(nullptr)
{

}

VulkanImpl::~VulkanImpl()
{
	_cleanup();
}

void VulkanImpl::copyBuffer(VkBuffer* dst, VkBuffer* src, VkDeviceSize size) const
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

	VkBufferCopy copy = {};
	copy.size = size;

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &buffer;

	vkBeginCommandBuffer(buffer, &begin);
	vkCmdCopyBuffer(buffer, *src, *dst, 1, &copy);
	vkEndCommandBuffer(buffer);

	vkQueueSubmit(_graphicsQueue.vkQueue, 1, &submit, VK_NULL_HANDLE);
	vkQueueWaitIdle(_graphicsQueue.vkQueue);

	vkFreeCommandBuffers(_device, _commandPool, 1, &buffer);
}

void VulkanImpl::createAndBindBuffer(const VkBufferCreateInfo& info, VkBuffer* buffer, VkDeviceMemory* memory, VkMemoryPropertyFlags flags) const
{
	if (!buffer || !memory)
		return;

	VkMemoryRequirements memReq;

	vkCreateBuffer(_device, &info, nullptr, buffer);
	vkGetBufferMemoryRequirements(VulkanImpl::device(), *buffer, &memReq);

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = getMemoryTypeIndex(memReq.memoryTypeBits, flags);

	vkAllocateMemory(VulkanImpl::device(), &alloc, nullptr, memory);

	vkBindBufferMemory(VulkanImpl::device(), *buffer, *memory, 0);
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

void VulkanImpl::init(const Window& window)
{
	_createInstance();

	//Register the debugger at the earliest opportunity, which is immediately after we have a VkInstance
	_registerDebugger();


	window.createSurface(_instance, &_surface);
	_initDevice();
	_createCommandPool();
	ShaderCache::init();
	_loadTestModel();

	_createRenderPass();
	_createSwapChain();
	_createLayouts();
	_createPipeline();
	_allocateCommandBuffers();
	_createSampler();
}

void VulkanImpl::render()
{
	_swapChain->present();
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

	VkClearValue clearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

	for (size_t i = 0; i < _commandBuffers.size(); i++)
	{
		VkCommandBuffer buffer = _commandBuffers[i];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.clearValueCount = 1;
		info.pClearValues = &clearColour;
		info.renderPass = _renderPass;
		info.renderArea.offset = { 0, 0 };
		info.renderArea.extent = _swapChain->surfaceCapabilities().currentExtent;
		info.framebuffer = framebuffers[i];

		vkBeginCommandBuffer(buffer, &beginInfo);

		vkCmdBeginRenderPass(buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		
		vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
		
		for (Model* model : _models)
		{
			model->draw(buffer);
		}

		vkCmdEndRenderPass(buffer);

		vkEndCommandBuffer(buffer);
	}
}

void VulkanImpl::_cleanup()
{
	vkDeviceWaitIdle(_device);

	for (Model* model : _models)
	{
		delete model;
	}

	_models.clear();

	vkDestroySampler(_device, _sampler, nullptr);
	ShaderCache::shutdown();
	vkDestroyPipeline(_device, _pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(_device, _descriptorLayout, nullptr);

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
	info.queueFamilyIndex = _graphicsQueue.index;

	if (vkCreateCommandPool(_device, &info, nullptr, &_commandPool) != VK_SUCCESS)
	{
		//
	}
}

void VulkanImpl::_createLayouts()
{
	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 0;

	if(vkCreateDescriptorSetLayout(_device, &info, nullptr, &_descriptorLayout) != VK_SUCCESS)
	{
		//
	}

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1;
	layoutCreateInfo.pSetLayouts = &_descriptorLayout;

	if (vkCreatePipelineLayout(_device, &layoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS)
	{
		//
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

void VulkanImpl::_createPipeline()
{
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[0].module = ShaderCache::getModule("shaders/triangle/triangle.vert");

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";
	stages[1].module = ShaderCache::getModule("shaders/triangle/triangle.frag");

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
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VkVertexInputBindingDescription vbs = {};
	vbs.binding = 0;
	vbs.stride = sizeof(Vertex);
	vbs.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vtxAttrs[2] = {};
	vtxAttrs[0].binding = 0;
	vtxAttrs[0].location = 0;
	vtxAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vtxAttrs[0].offset = offsetof(Vertex, position);

	vtxAttrs[1].binding = 0;
	vtxAttrs[1].location = 1;
	vtxAttrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vtxAttrs[1].offset = offsetof(Vertex, color);

	VkPipelineVertexInputStateCreateInfo vis = {};
	vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vis.vertexBindingDescriptionCount = 1;
	vis.pVertexBindingDescriptions = &vbs;
	vis.vertexAttributeDescriptionCount = 2;
	vis.pVertexAttributeDescriptions = vtxAttrs;

	VkExtent2D extent = _swapChain->surfaceCapabilities().currentExtent;
	VkRect2D sc = {
		0, 0,
		(int32_t)extent.width, (int32_t)extent.height
	};

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

	if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &_pipeline) != VK_SUCCESS)
	{
		//
	}
}

void VulkanImpl::_createRenderPass()
{
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

	VkSubpassDependency dependency = {};
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;

	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstSubpass = 0;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachDesc;
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

void VulkanImpl::_loadTestModel()
{
	Model* model = new Model("triangle", this);
	_models.push_back(model);
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