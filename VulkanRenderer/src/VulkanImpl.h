#ifndef VULKAN_IMPL_H_
#define VULKAN_IMPL_H_

#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_map>

#include "Window.h"
#include "Buffer.h"
#include "VulkanUtil.h"
#include "SetBinding.h"
#include "renderpass/RenderPass.h"

const std::string ASSET_PATH = "assets/";

class Model;
class Scene;
class SwapChain;

struct Uniform
{
	//Local to the device, not CPU-mappable
	Buffer localBuffer;

	//CPU-mappable buffer
	Buffer stagingBuffer;

	//Total size of the entire buffer
	VkDeviceSize size;

	//For dynamic buffers, the range of an individual binding.
	//For static buffers, this is the same as Uniform::size
	VkDeviceSize range;
};

typedef class VulkanImpl
{
public:
	VulkanImpl();
	~VulkanImpl();

	void addRenderPass(RenderPass* renderPass);

	void copyBuffer(const Buffer& dst, const Buffer& src, VkDeviceSize size, VkDeviceSize offset = 0) const;

	void clearShaderCache();

	void createAndBindBuffer(const VkBufferCreateInfo& info, Buffer& buffer, VkMemoryPropertyFlags flags) const;

	Uniform* createUniform(const std::string& name, size_t size, size_t range = 0);

	void destroyPipelines();

	size_t getAlignedRange(size_t needed) const;

	uint32_t getMemoryTypeIndex(uint32_t bits, VkMemoryPropertyFlags flags) const;

	RenderPass* getRenderPass(RenderPassType type) const;

	Uniform* getUniform(const std::string& name);

	void init(const Window& window);

	void recordCommandBuffers(const Scene* scene = 0);

	void recreateSwapChain(uint32_t width = 0, uint32_t height = 0);

	void render();

	void setImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange& range) const;

	VkCommandBuffer startOneShotCmdBuffer() const;

	void submitOneShotCmdBuffer(VkCommandBuffer buffer) const;

	void updateUniform(const std::string& name, void* data, size_t size, size_t offset = 0);

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

	inline VkExtent2D extent() const
	{
		return _extent;
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

	inline const VkPhysicalDeviceProperties properties() const
	{
		return _physicalProperties;
	}

	inline VkSampler sampler() const
	{
		return _sampler;
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
	std::vector<RenderPass*> _renderPasses;
	std::unordered_map<std::string, Uniform*> _uniforms;

	static VkDevice _device;
	static VkPhysicalDevice _physicalDevice;
	VkDebugReportCallbackEXT _debugCallback;
	VkInstance _instance;
	VkSurfaceKHR _surface;
	VkSampler _sampler;
	VkCommandPool _commandPool;
	VkExtent2D _extent;
	VkPhysicalDeviceProperties _physicalProperties;

	QueueInfo _graphicsQueue;
	QueueInfo _presentQueue;

	SwapChain* _swapChain;

	void _allocateCommandBuffers();
	void _cleanup();
	void _createCommandPool();
	void _createInstance();
	void _createSampler();
	void _createSwapChain();
	void _createUniforms();
	void _initDevice();
	VkPhysicalDevice _pickPhysicalDevice();
	void _queryDeviceQueueFamilies(VkPhysicalDevice device);
	void _registerDebugger();
} Renderer;

#endif //VULKAN_IMPL_H_