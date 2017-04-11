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
};

#endif //SCREEN_RENDER_PASS_H_