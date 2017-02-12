#include "Renderer.h"
#include "SwapChain.h"
#include "ShaderCache.h"
#include "texture/TextureCache.h"
#include "Model.h"
#include "Camera.h"
#include "Scene.h"
#include "renderpass/ShadowMapRenderPass.h"

#include <set>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

VkDevice Renderer::_device = VK_NULL_HANDLE;
VkPhysicalDevice Renderer::_physicalDevice = VK_NULL_HANDLE;

//TODO: don't hardcode this and recreate the pool if necessary
const int MAX_TEXTURES = 64;
const int MAX_MODELS = 64;
const int MAX_MATERIALS = 64;

Renderer::Renderer() : _swapChain(nullptr)
{

}

Renderer::~Renderer()
{
	_cleanup();
}

void Renderer::addRenderPass(RenderPass* renderPass)
{
	_renderPasses.push_back(renderPass);
}

void Renderer::copyBuffer(const Buffer& dst, const Buffer& src, VkDeviceSize size, VkDeviceSize offset) const
{
	VkCommandBuffer buffer = startOneShotCmdBuffer();

	VkBufferCopy copy = {};
	copy.size = size;
	copy.dstOffset = copy.srcOffset = offset;

	vkCmdCopyBuffer(buffer, src.buffer, dst.buffer, 1, &copy);
	
	submitOneShotCmdBuffer(buffer);
}

void Renderer::clearShaderCache()
{
	ShaderCache::clear();
}

void Renderer::createAndBindBuffer(const VkBufferCreateInfo& info, Buffer& buffer, VkMemoryPropertyFlags flags) const
{
	VkMemoryRequirements memReq;

	VkCheck(vkCreateBuffer(_device, &info, nullptr, &buffer.buffer));
	vkGetBufferMemoryRequirements(Renderer::device(), buffer.buffer, &memReq);

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = getMemoryTypeIndex(memReq.memoryTypeBits, flags);

	VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &buffer.memory));
	VkCheck(vkBindBufferMemory(Renderer::device(), buffer.buffer, buffer.memory, 0));
}

Uniform* Renderer::createUniform(const std::string& name, size_t size, size_t range)
{
	if (_uniforms.find(name) != _uniforms.end())
	{
		//TODO: delete uniform then continue to recreate it?
		return _uniforms[name];
	}

	Uniform* uniform = new Uniform;
	_uniforms[name] = uniform;
	
	uniform->size = size;
	uniform->range = (range > 0 ? range : size);

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

void Renderer::destroyPipelines()
{
	vkDeviceWaitIdle(_device);

	for (RenderPass* p : _renderPasses)
		p->destroyPipelines();
}

size_t Renderer::getAlignedRange(size_t needed) const
{
	size_t min = _physicalProperties.limits.minUniformBufferOffsetAlignment;
	size_t mod = needed % min;
	size_t range = (min * (needed / min)) + (needed % min ? min : 0);

	return range;
}

uint32_t Renderer::getMemoryTypeIndex(uint32_t bits, VkMemoryPropertyFlags flags) const
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

RenderPass* Renderer::getRenderPass(RenderPassType type) const
{
	for (RenderPass* p : _renderPasses)
	{
		if (p->type() == type)
			return p;
	}

	return nullptr;
}

Uniform* Renderer::getUniform(const std::string& name)
{
	if (_uniforms.find(name) != _uniforms.end())
		return _uniforms[name];

	//TODO: return createUniform?
	return nullptr;
}

void Renderer::init(const Window& window)
{
	_createInstance();

	//Register the debugger at the earliest opportunity, which is immediately after we have a VkInstance
	_registerDebugger();


	window.createSurface(_instance, &_surface);
	_initDevice();
	_createCommandPool();
	ShaderCache::init();
	TextureCache::init();
	_createSwapChain();
	_createSampler();
	_createUniforms();
}

void Renderer::recordCommandBuffers(const Scene* scene)
{
	//TODO: use fences properly instead
	vkDeviceWaitIdle(_device);

	const std::vector<VkFramebuffer> framebuffers = _swapChain->framebuffers();
	
	for (size_t i = 0; i < _commandBuffers.size(); i++)
	{
		VkCommandBuffer buffer = _commandBuffers[i];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkCheck(vkBeginCommandBuffer(buffer, &beginInfo));

		for (RenderPass* pass : _renderPasses)
			pass->render(buffer, framebuffers[i]);

		VkCheck(vkEndCommandBuffer(buffer));
	}
}

void Renderer::recreateSwapChain(uint32_t width, uint32_t height)
{
	vkDeviceWaitIdle(_device);
	_swapChain->resize(width, height);
	_allocateCommandBuffers();
}

void Renderer::render()
{
	_swapChain->present();
}

void Renderer::setImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange& range) const
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

VkCommandBuffer Renderer::startOneShotCmdBuffer() const
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

	VkCheck(vkBeginCommandBuffer(buffer, &begin));

	return buffer;
}

void Renderer::submitOneShotCmdBuffer(VkCommandBuffer buffer) const
{
	VkCheck(vkEndCommandBuffer(buffer));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &buffer;

	VkCheck(vkQueueSubmit(_graphicsQueue.vkQueue, 1, &submit, VK_NULL_HANDLE));
	VkCheck(vkQueueWaitIdle(_graphicsQueue.vkQueue));

	vkFreeCommandBuffers(_device, _commandPool, 1, &buffer);
}

void Renderer::updateUniform(const std::string& name, void* data, size_t size, size_t offset)
{
	if (_uniforms.find(name) == _uniforms.end())
		return;

	Uniform* uniform = _uniforms[name];
	uniform->stagingBuffer.copyData(data, size, offset);
	copyBuffer(uniform->localBuffer, uniform->stagingBuffer, size, offset);
}

void Renderer::_allocateCommandBuffers()
{
	if (_commandBuffers.size())
		_commandBuffers.clear();

	const std::vector<VkFramebuffer> framebuffers = _swapChain->framebuffers();
	_commandBuffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandBufferCount = (uint32_t)_commandBuffers.size();
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandPool = _commandPool;

	VkCheck(vkAllocateCommandBuffers(_device, &info, _commandBuffers.data()));

	recordCommandBuffers();
}

void Renderer::_cleanup()
{
	VkCheck(vkDeviceWaitIdle(_device));

	for (auto pair : _uniforms)
	{
		delete pair.second;
	}

	_uniforms.clear();

	vkDestroySampler(_device, _sampler, nullptr);
	ShaderCache::clear();
	TextureCache::shutdown();

	destroyPipelines();

	for (RenderPass* p : _renderPasses)
	{
		delete p;
	}
	_renderPasses.clear();

	delete _swapChain;
	_swapChain = nullptr;

	vkDestroyCommandPool(_device, _commandPool, nullptr);
	vkDestroyDevice(_device, nullptr);
	_device = VK_NULL_HANDLE;
	vkDestroySurfaceKHR(_instance, _surface, nullptr);

	if (VulkanUtil::DEBUGENABLE)
	{
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
			(PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
		if (vkDestroyDebugReportCallbackEXT)
			vkDestroyDebugReportCallbackEXT(_instance, _debugCallback, nullptr);
	}

	vkDestroyInstance(_instance, nullptr);
}

void Renderer::_createCommandPool()
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = _graphicsQueue.index;

	VkCheck(vkCreateCommandPool(_device, &info, nullptr, &_commandPool));
}

void Renderer::_createInstance()
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
	VulkanUtil::getRequiredExtensions(extensions);
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (VulkanUtil::DEBUGENABLE)
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
	
	VkCheck(vkCreateInstance(&createInfo, nullptr, &_instance));
}

void Renderer::_createSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;

	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;

	VkCheck(vkCreateSampler(_device, &samplerInfo, nullptr, &_sampler));
}

void Renderer::_createSwapChain()
{
	_swapChain = new SwapChain(*this);
	_swapChain->init(_surface);
	_extent = _swapChain->surfaceCapabilities().currentExtent;
}

void Renderer::_createUniforms()
{
	createUniform("camera", sizeof(CameraUniform));
	createUniform("model", getAlignedRange(sizeof(ModelUniform)) * MAX_MODELS);
	createUniform("light", sizeof(Light));
	createUniform("material", getAlignedRange(sizeof(MaterialData)) * MAX_MODELS);
}

void Renderer::_initDevice()
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

	if (VulkanUtil::DEBUGENABLE)
	{
		info.enabledLayerCount = (uint32_t)VulkanUtil::VALIDATION_LAYERS.size();
		info.ppEnabledLayerNames = VulkanUtil::VALIDATION_LAYERS.data();
	}

	VkCheck(vkCreateDevice(_physicalDevice, &info, nullptr, &_device));

	vkGetDeviceQueue(_device, _graphicsQueue.index, 0, &_graphicsQueue.vkQueue);
	vkGetDeviceQueue(_device, _presentQueue.index, 0, &_presentQueue.vkQueue);
}

VkPhysicalDevice Renderer::_pickPhysicalDevice()
{
	uint32_t physicalDeviceCount;
	VkCheck(vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr));

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	if (physicalDeviceCount > 0)
	{
		VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[physicalDeviceCount];
		VkCheck(vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices));
		physicalDevice = physicalDevices[0]; //default to any GPU in case it's all we have.


		//Pick the first discrete GPU we encounter.
		for (uint32_t i = 0; i < physicalDeviceCount; i++)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = physicalDevices[i];
				_physicalProperties = properties;
				break;
			}
		}

		delete[] physicalDevices;
	}

	return physicalDevice;
}

void Renderer::_queryDeviceQueueFamilies(VkPhysicalDevice device)
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
		VkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport));

		if (families[i].queueCount > 0 && presentSupport) {
			_presentQueue.index = i;
		}

		if (_graphicsQueue.index != -1 && _presentQueue.index != -1)
			break;
	}
}

void Renderer::_registerDebugger()
{
	if (!VulkanUtil::DEBUGENABLE) return;

	VkDebugReportCallbackCreateInfoEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	info.pfnCallback = VulkanUtil::debugCallback;
	info.pUserData = (void*)this;
	info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = 
		(PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugReportCallbackEXT");
	if(vkCreateDebugReportCallbackEXT)
		vkCreateDebugReportCallbackEXT(_instance, &info, nullptr, &_debugCallback);
}