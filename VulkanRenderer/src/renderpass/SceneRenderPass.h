#ifndef SCENE_RENDER_PASS_H_
#define SCENE_RENDER_PASS_H_

#include "RenderPass.h"

class Scene;

class SceneRenderPass : public RenderPass
{
public:
	SceneRenderPass(Scene& scene, RenderPass& shadowPass) 
		: _scene(&scene), _shadowPass(&shadowPass) {}

	~SceneRenderPass();

	void SceneRenderPass::allocateTextureDescriptor(VkDescriptorSet& set, SetBinding binding = SET_BINDING_TEXTURE);

	virtual void init(VulkanImpl* renderer) override;

	virtual void render(VkCommandBuffer cmd, VkFramebuffer framebuffer = VK_NULL_HANDLE) override;

	inline virtual RenderPassType type() {
		return RenderPassType::SCENE;
	};

protected:
	virtual void _createDescriptorSets(VulkanImpl* renderer) override;

	virtual void _createPipeline(const std::string& shaderName) override;

	virtual void _createPipelineLayout() override;

	virtual void _createRenderPass() override;

private:
	Scene* _scene;

	RenderPass* _shadowPass;

	VkExtent2D _extent;
};

#endif //SCENE_RENDER_PASS_H_