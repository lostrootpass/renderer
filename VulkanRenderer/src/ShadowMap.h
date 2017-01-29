#ifndef SHADOW_MAP_H_
#define SHADOW_MAP_H_

#include "VulkanImpl.h"

class ShadowMap
{
public:
	ShadowMap(VulkanImpl& renderer);
	~ShadowMap();

	void render(VkCommandBuffer cmd, const class Scene* scene = nullptr) const;

	VkDescriptorSet set() const
	{
		return _set;
	}

private:
	VkRenderPass _renderPass;
	VkFramebuffer _framebuffer;

	VkImageView _depthView;
	VkImage _depthImage;
	VkDeviceMemory _depthMemory;

	VkDescriptorSet _set;

	VulkanImpl* _renderer;

	void _createDepthBuffer(VulkanImpl* renderer);
	void _createFramebuffer();
	void _createRenderPass();
	void _init(VulkanImpl* renderer);
};

#endif //SHADOW_MAP_H_