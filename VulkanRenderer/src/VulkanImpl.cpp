#include "VulkanImpl.h"
#include "VulkanImplUtil.h"
#include "SwapChain.h"

#include <set>

VulkanImpl::VulkanImpl() : _swapChain(nullptr)
{

}

VulkanImpl::~VulkanImpl()
{
	_cleanup();
}

void VulkanImpl::init(const Window& window)
{
	_createInstance();

	//Register the debugger at the earliest opportunity, which is immediately after we have a VkInstance
	_registerDebugger();


	window.createSurface(_instance, &_surface);
	_initDevice();
	_createRenderPass();
	_createSwapChain();
	_createCommandPool();
	_allocateCommandBuffers();
	_createPipeline();
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

	VkClearValue clearColour = { 1.0f, 0.0f, 0.0f, 1.0f };

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
		//vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
		vkCmdEndRenderPass(buffer);

		vkEndCommandBuffer(buffer);
	}
}

void VulkanImpl::_cleanup()
{
	vkDeviceWaitIdle(_device);

	vkDestroySampler(_device, _sampler, nullptr);

	//vkDestroyPipeline(_device, _pipeline, nullptr);

	delete _swapChain;
	_swapChain = nullptr;
	
	vkDestroyRenderPass(_device, _renderPass, nullptr);
	vkDestroyCommandPool(_device, _commandPool, nullptr);
	vkDestroyDevice(_device, nullptr);
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