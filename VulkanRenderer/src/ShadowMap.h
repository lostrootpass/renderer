#ifndef SHADOW_MAP_H_
#define SHADOW_MAP_H_

#include "VulkanImpl.h"
#include "texture/Texture.h"

class ShadowMap
{
public:
	ShadowMap(VulkanImpl& renderer);
	~ShadowMap();

	void render(VkCommandBuffer cmd, const class Scene* scene = nullptr) const;

	VkDescriptorSet set() const
	{
		return _depthTexture->set();
	}

private:
	VkRenderPass _renderPass;
	VkFramebuffer _framebuffer;

	Texture* _depthTexture;

	void _createFramebuffer();
	void _createRenderPass();
	void _init(VulkanImpl* renderer);
};

#endif //SHADOW_MAP_H_