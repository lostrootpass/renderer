#ifndef VULKAN_IMPL_H_
#define VULKAN_IMPL_H_

#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_map>

#include "Window.h"
#include "Buffer.h"
#include "VulkanUtil.h"
#include "SetBinding.h"

const std::string ASSET_PATH = "assets/";

class Model;
class Scene;
class SwapChain;
class ShadowMap;

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

	void allocateTextureDescriptor(VkDescriptorSet& set, SetBinding binding = SET_BINDING_TEXTURE);

	void bindDescriptorSet(VkCommandBuffer cmd, SetBinding index, const VkDescriptorSet& set, bool offscreen = false) const;

	void bindDescriptorSetById(VkCommandBuffer cmd, SetBinding set, std::vector<uint32_t>* offsets = nullptr, bool offscreen = false) const;

	void copyBuffer(const Buffer& dst, const Buffer& src, VkDeviceSize size, VkDeviceSize offset = 0) const;

	void createAndBindBuffer(const VkBufferCreateInfo& info, Buffer& buffer, VkMemoryPropertyFlags flags) const;

	Uniform* createUniform(const std::string& name, size_t size, size_t range = 0);

	size_t getAlignedRange(size_t needed) const;

	uint32_t getMemoryTypeIndex(uint32_t bits, VkMemoryPropertyFlags flags) const;

	const VkPipeline getPipelineForShader(const std::string& shaderName, bool useOffscreenLayout = false);

	void init(const Window& window);

	void recordCommandBuffers(const Scene* scene = 0);

	void render();

	void setImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange& range) const;

	VkCommandBuffer startOneShotCmdBuffer() const;

	void submitOneShotCmdBuffer(VkCommandBuffer buffer) const;

	void updateSampledImage(VkImageView view) const;

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

	inline VkDescriptorSet getDescriptorSet(SetBinding set) const
	{
		return _descriptorSets[set];
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

	void setOffscreenPass(VkRenderPass p)
	{
		_offscreenPass = p;
	}

private:
	struct QueueInfo
	{
		VkQueue vkQueue;
		uint32_t index;
	};

	std::vector<VkCommandBuffer> _commandBuffers;
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
	VkRenderPass _offscreenPass;
	VkPipelineLayout _pipelineLayout;
	VkPipelineLayout _offscreenLayout;
	VkSampler _sampler;
	VkCommandPool _commandPool;
	VkDescriptorPool _descriptorPool;
	VkExtent2D _extent;
	VkPhysicalDeviceProperties _physicalProperties;

	QueueInfo _graphicsQueue;
	QueueInfo _presentQueue;

	SwapChain* _swapChain;
	ShadowMap* _shadowMap;

	void _allocateCommandBuffers();
	void _cleanup();
	void _createCommandPool();
	void _createDescriptorSets();
	void _createInstance();
	void _createLayouts();
	void _createRenderPass();
	void _createSampler();
	void _createSwapChain();
	void _initDevice();
	VkPhysicalDevice _pickPhysicalDevice();
	void _queryDeviceQueueFamilies(VkPhysicalDevice device);
	void _registerDebugger();
} Renderer;

#endif //VULKAN_IMPL_H_