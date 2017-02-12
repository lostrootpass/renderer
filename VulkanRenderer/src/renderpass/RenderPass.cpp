#include "RenderPass.h"
#include "../Renderer.h"

RenderPass::~RenderPass()
{
	vkDestroyRenderPass(Renderer::device(), _renderPass, nullptr);
	vkDestroyPipelineLayout(Renderer::device(), _pipelineLayout, nullptr);
	vkDestroyDescriptorPool(Renderer::device(), _descriptorPool, nullptr);

	destroyPipelines();

	for (VkDescriptorSetLayout& layout : _descriptorLayouts)
		vkDestroyDescriptorSetLayout(Renderer::device(), layout, nullptr);
}

void RenderPass::bindDescriptorSet(VkCommandBuffer cmd, SetBinding index, const VkDescriptorSet& set) const
{
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, index, 1, &set, 0, nullptr);
}

void RenderPass::bindDescriptorSetById(VkCommandBuffer cmd, SetBinding set, std::vector<uint32_t>* offsets) const
{
	if (set > SET_BINDING_COUNT)
		return;

	uint32_t offsetCount = (offsets ? (uint32_t)offsets->size() : 0);
	const uint32_t* offsetData = (offsets ? offsets->data() : nullptr);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, set, 1, &_descriptorSets[set], offsetCount, offsetData);
}

void RenderPass::destroyPipelines()
{
	for (auto& pair : _pipelines)
		vkDestroyPipeline(Renderer::device(), pair.second, nullptr);

	_pipelines.clear();
}

void RenderPass::updatePushConstants(VkCommandBuffer cmd, size_t size, void* data) const
{
	vkCmdPushConstants(cmd, _pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)size, data);
}
