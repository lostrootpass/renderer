#ifndef SCREEN_RENDER_PASS_H_
#define SCREEN_RENDER_PASS_H_

#include "RenderPass.h"
#include "../Scene.h"

#include <unordered_map>

class Renderer;

class PostProcessRenderPass : public RenderPass
{
public:
	PostProcessRenderPass(Scene& scene);

	~PostProcessRenderPass();

	void addEffect(const std::string& shaderName);

	virtual void init(Renderer* renderer) override;

	virtual void render(VkCommandBuffer cmd, const Framebuffer* framebuffer = nullptr) override;

	inline void recreateDescriptorSets(Renderer* renderer)
	{
		_createDescriptorSets(renderer);
	}

	inline virtual RenderPassType type() {
		return RenderPassType::POSTPROCESS;
	};

protected:
	virtual void _createDescriptorSets(Renderer* renderer) override;

	virtual void _createPipeline(const std::string& shaderName) override;

	virtual void _createPipelineLayout() override;

	virtual void _createRenderPass() override;

private:
	Scene* _scene;

	Renderer* _renderer;

	VkSampler _sampler;

	std::unordered_map<VkImageView, VkDescriptorSet> _imageViewSets;
	std::unordered_map<VkFramebuffer, Framebuffer> _postprocessRenderTargets;
	std::unordered_map<VkFramebuffer, const Framebuffer*> _postprocessBackbuffers;
	std::vector<std::string> _passes;

	void _allocatePostprocessRenderTargets(Renderer* renderer);

	void _destroyPostprocessRenderTargets();
};

#endif //SCREEN_RENDER_PASS_H_