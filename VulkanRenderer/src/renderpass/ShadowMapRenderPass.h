#ifndef SHADOW_MAP_RENDER_PASS_H_
#define SHADOW_MAP_RENDER_PASS_H_

#include "RenderPass.h"

class Scene;

class ShadowMapRenderPass : public RenderPass
{
public:
	ShadowMapRenderPass(Scene& scene) : _scene(&scene), _depthTexture(nullptr) {}

	~ShadowMapRenderPass();

	virtual void init(VulkanImpl* renderer) override;

	void recreateShadowMap(VulkanImpl* renderer);

	virtual void render(VkCommandBuffer cmd, VkFramebuffer) override;

	inline VkDescriptorSet set() const
	{
		return _depthTexture ? _depthTexture->set() : VK_NULL_HANDLE;
	}

	inline virtual RenderPassType type() {
		return RenderPassType::SHADOWMAP;
	};

protected:
	virtual void _createDescriptorSets(VulkanImpl* renderer) override;

	virtual void _createPipeline(const std::string& shaderName) override;

	virtual void _createPipelineLayout() override;

	virtual void _createRenderPass() override;

private:
	VkFramebuffer _framebuffer;

	Scene* _scene;

	Texture* _depthTexture;

	void _createFramebuffer();
};

#endif //SHADOW_MAP_RENDER_PASS_H_