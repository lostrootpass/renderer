#ifndef VULKAN_IMPL_H_
#define VULKAN_IMPL_H_

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_map>

#include "Window.h"
#include "Buffer.h"

class Model;

struct Uniform
{
	//Local to the device, not CPU-mappable
	Buffer localBuffer;

	//CPU-mappable buffer
	Buffer stagingBuffer;
};

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

	void copyBuffer(const Buffer& dst, const Buffer& src, VkDeviceSize size) const;

	void createAndBindBuffer(const VkBufferCreateInfo& info, Buffer& buffer, VkMemoryPropertyFlags flags) const;

	uint32_t getMemoryTypeIndex(uint32_t bits, VkMemoryPropertyFlags flags) const;

	const VkPipeline getPipelineForShader(const std::string& shaderName);

	void init(const Window& window);

	void render();

	void setImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange& range) const;

	VkCommandBuffer startOneShotCmdBuffer() const;

	void submitOneShotCmdBuffer(VkCommandBuffer buffer) const;

	void updateUniform(const std::string& name, void* data, size_t size);

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
	std::vector<Model*> _models;
	std::vector<VkDescriptorSetLayout> _descriptorLayouts;
	std::vector<VkDescriptorSet> _descriptorSets;
	std::unordered_map<std::string, VkPipeline> _pipelines;
	std::unordered_map<std::string, Uniform*> _uniforms;

	static VkDevice _device;
	static VkPhysicalDevice _physicalDevice;
	VkDebugReportCallbackEXT _debugCallback;
	VkInstance _instance;
	VkSurfaceKHR _surface;
	VkRenderPass _renderPass;
	VkPipelineLayout _pipelineLayout;
	VkSampler _sampler;
	VkCommandPool _commandPool;
	VkDescriptorPool _descriptorPool;

	QueueInfo _graphicsQueue;
	QueueInfo _presentQueue;

	class SwapChain* _swapChain;
	struct Camera* _camera;

	void _allocateCommandBuffers();
	void _cleanup();
	void _createCommandPool();
	void _createDescriptorSets();
	void _createInstance();
	void _createLayouts();
	void _createRenderPass();
	void _createSampler();
	void _createSwapChain();
	void _createUniforms();
	void _initDevice();
	void _loadTestModel();
	VkPhysicalDevice _pickPhysicalDevice();
	void _queryDeviceQueueFamilies(VkPhysicalDevice device);
	void _registerDebugger();
} Renderer;

#endif //VULKAN_IMPL_H_