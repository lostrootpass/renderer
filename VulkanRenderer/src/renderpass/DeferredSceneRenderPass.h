#ifndef DEFERRED_SCENE_RENDER_PASS_H_
#define DEFERRED_SCENE_RENDER_PASS_H_

#include "RenderPass.h"

class Scene;
class SSAORenderPass;

class DeferredSceneRenderPass : public RenderPass
{
public:
	DeferredSceneRenderPass(Scene& scene, RenderPass& shadowPass) 
		: _scene(&scene), _shadowPass(&shadowPass), _ssaoPass(nullptr) {}

	~DeferredSceneRenderPass();

	void allocateTextureDescriptor(VkDescriptorSet& set, SetBinding binding = SET_BINDING_TEXTURE);

	virtual void init(Renderer* renderer) override;

	virtual void reload() override;

	virtual void render(VkCommandBuffer cmd, const Framebuffer* framebuffer = nullptr) override;

	virtual void resize(uint32_t width, uint32_t height) override;

	inline virtual RenderPassType type() {
		return RenderPassType::SCENE;
	};

protected:
	virtual void _createDescriptorSets(Renderer* renderer) override;

	virtual void _createPipeline(const std::string& shaderName) override;

	virtual void _createPipelineLayout() override;

	virtual void _createRenderPass() override;

private:
	void _cleanupDeferredTargets();

	void _createRenderTargets(Renderer* renderer);

	void _createDeferredLayout();

	void _createDeferredPipeline();

	struct DeferredFramebuffer : public Framebuffer
	{
		VkImage normalImage;
		VkImageView normalView;
		VkDeviceMemory normalMemory;
	};

	std::vector<DeferredFramebuffer> _deferredFramebuffers;

	std::vector<VkDescriptorSetLayout> _deferredSetLayouts;

	std::vector<VkDescriptorSet> _deferredSets;

	Renderer* _renderer;

	Scene* _scene;

	SSAORenderPass* _ssaoPass;

	RenderPass* _shadowPass;

	VkExtent2D _extent;
	
	VkSampler _sampler;

	VkDescriptorSet _bindingSet;

	VkDescriptorSetLayout _deferredSetLayout;

	VkPipelineLayout _deferredPipelineLayout;

	VkPipeline _deferredPipeline;

	VkRenderPass _geometryPass;
};



#endif