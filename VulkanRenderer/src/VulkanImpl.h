#ifndef VULKAN_IMPL_H_
#define VULKAN_IMPL_H_

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>

#include "Window.h"

typedef class VulkanImpl
{
public:
	struct Util;

#ifndef NDEBUG
	static const bool DEBUGENABLE = true;
#else
	static const bool DEBUGENABLE = false;
#endif

	VulkanImpl();
	~VulkanImpl();

	void init(const Window& window);

	void render();

	inline const VkCommandBuffer commandBuffer(size_t idx) const
	{
		if (idx >= _commandBuffers.size())
			return VK_NULL_HANDLE;

		return _commandBuffers[idx];
	}

	inline const VkDevice device() const
	{
		return _device;
	}

	inline const VkQueue graphicsQueue() const
	{
		return _graphicsQueue.vkQueue;
	}

	inline const VkInstance instance() const
	{
		return _instance;
	}

	inline const VkPhysicalDevice physicalDevice() const
	{
		return _physicalDevice;
	}

	inline const VkQueue presentQueue() const
	{
		return _presentQueue.vkQueue;
	}

	inline const VkRenderPass renderPass() const
	{
		return _renderPass;
	}

	inline const VkSurfaceKHR surface() const
	{
		return _surface;
	}

private:
	struct QueueInfo
	{
		VkQueue vkQueue;
		uint32_t index;
	};

	std::vector<VkCommandBuffer> _commandBuffers;

	VkDebugReportCallbackEXT _debugCallback;
	VkInstance _instance;
	VkSurfaceKHR _surface;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkRenderPass _renderPass;
	VkPipeline _pipeline;
	VkSampler _sampler;
	VkCommandPool _commandPool;

	QueueInfo _graphicsQueue;
	QueueInfo _presentQueue;

	class SwapChain* _swapChain;

	void _allocateCommandBuffers();
	void _cleanup();
	void _createCommandPool();
	void _createInstance();
	void _createPipeline();
	void _createRenderPass();
	void _createSampler();
	void _createSwapChain();
	void _initDevice();
	VkPhysicalDevice _pickPhysicalDevice();
	void _queryDeviceQueueFamilies(VkPhysicalDevice device);
	void _registerDebugger();
} Renderer;

#endif //VULKAN_IMPL_H_