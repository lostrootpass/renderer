#ifndef VULKAN_IMPL_H_
#define VULKAN_IMPL_H_

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>

#include "Window.h"
#include "Model.h"

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

	void copyBuffer(VkBuffer* dst, VkBuffer* src, VkDeviceSize size) const;

	void createAndBindBuffer(const VkBufferCreateInfo& info, VkBuffer* buffer, VkDeviceMemory* memory, VkMemoryPropertyFlags flags) const;

	uint32_t getMemoryTypeIndex(uint32_t bits, VkMemoryPropertyFlags flags) const;

	void init(const Window& window);

	void render();

	inline const VkCommandBuffer commandBuffer(size_t idx) const
	{
		if (idx >= _commandBuffers.size())
			return VK_NULL_HANDLE;

		return _commandBuffers[idx];
	}

	inline static const VkDevice device()
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

	inline static const VkPhysicalDevice physicalDevice()
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
	std::vector<Model*> _models;

	static VkDevice _device;
	static VkPhysicalDevice _physicalDevice;
	VkDebugReportCallbackEXT _debugCallback;
	VkInstance _instance;
	VkSurfaceKHR _surface;
	VkRenderPass _renderPass;
	VkPipelineLayout _pipelineLayout;
	VkPipeline _pipeline;
	VkDescriptorSetLayout _descriptorLayout;
	VkDescriptorSet _descriptor;
	VkSampler _sampler;
	VkCommandPool _commandPool;

	QueueInfo _graphicsQueue;
	QueueInfo _presentQueue;

	class SwapChain* _swapChain;

	void _allocateCommandBuffers();
	void _cleanup();
	void _createCommandPool();
	void _createInstance();
	void _createLayouts();
	void _createPipeline();
	void _createRenderPass();
	void _createSampler();
	void _createSwapChain();
	void _initDevice();
	void _loadTestModel();
	VkPhysicalDevice _pickPhysicalDevice();
	void _queryDeviceQueueFamilies(VkPhysicalDevice device);
	void _registerDebugger();
} Renderer;

#endif //VULKAN_IMPL_H_