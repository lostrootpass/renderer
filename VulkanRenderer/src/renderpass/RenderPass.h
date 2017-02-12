#ifndef RENDER_PASS_H_
#define RENDER_PASS_H_

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include "../texture/Texture.h"

enum class RenderPassType
{
	SHADOWMAP,
	SCENE
};

class VulkanImpl;

class RenderPass
{
public:
	virtual ~RenderPass();

	void bindDescriptorSet(VkCommandBuffer cmd, SetBinding index, const VkDescriptorSet& set) const;

	void bindDescriptorSetById(VkCommandBuffer cmd, SetBinding set, std::vector<uint32_t>* offsets = nullptr) const;

	void destroyPipelines();

	VkPipeline getPipelineForShader(const std::string& shaderName)
	{
		if (_pipelines.find(shaderName) == _pipelines.end())
			_createPipeline(shaderName);

		return _pipelines[shaderName];
	};

	virtual void init(VulkanImpl* renderer) = 0;

	virtual void render(VkCommandBuffer cmd, VkFramebuffer framebuffer = VK_NULL_HANDLE) = 0;

	void updatePushConstants(VkCommandBuffer cmd, size_t size, void* data) const;

	inline VkRenderPass renderPass() const
	{
		return _renderPass;
	}

	inline virtual RenderPassType type() = 0;

protected:
	VkRenderPass _renderPass;
	VkPipelineLayout _pipelineLayout;
	VkDescriptorPool _descriptorPool;

	std::vector<VkDescriptorSetLayout> _descriptorLayouts;
	std::vector<VkDescriptorSet> _descriptorSets;

	std::unordered_map<std::string, VkPipeline> _pipelines;

	virtual void _createDescriptorSets(VulkanImpl* renderer) = 0;

	virtual void _createPipeline(const std::string& shaderName) = 0;

	virtual void _createPipelineLayout() = 0;

	virtual void _createRenderPass() = 0;
};

#endif //RENDER_PASS_H_