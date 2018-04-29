#ifndef SSAO_RENDER_PASS_H_
#define SSAO_RENDER_PASS_H_

#include "RenderPass.h"

class Texture;

class SSAORenderPass final : public RenderPass 
{
public:
	SSAORenderPass() {};

	~SSAORenderPass();

	virtual void init(Renderer* renderer) override;

	virtual void reload() override;

	virtual void render(VkCommandBuffer cmd, const Framebuffer* framebuffer = nullptr) override;

	virtual void resize(uint32_t width, uint32_t height) override;

	inline void setAttachmentBinding(VkDescriptorSet set)
	{
		_attachmentBinding = set;
	}

	inline VkImageView ssaoView() const
	{
		return _blurFramebuffer.view;
	}

	virtual RenderPassType type() override
	{
		return RenderPassType::POSTPROCESS;
	}

protected:

	virtual void _createDescriptorSets(Renderer* renderer) override;

	virtual void _createPipeline(const std::string& shaderName) override;

	virtual void _createPipelineLayout() override;

	virtual void _createRenderPass() override;

private:
	Framebuffer _ssaoFramebuffer;
	Framebuffer _blurFramebuffer;

	Texture* _noiseTexture;
	Renderer* _renderer;
	
	//TODO: do these need to be separate? 
	VkRenderPass _ssaoPass;
	VkRenderPass _blurPass;

	VkPipeline _ssaoPipeline;
	VkPipeline _blurPipeline;

	VkDescriptorSet _attachmentBinding;
	VkDescriptorSet _kernelNoiseSet;
	VkDescriptorSet _blurInputSet;

	VkSampler _sampler;

	void _cleanup();

	void _createNoiseTexture();

	void _createRenderTargets();

	void _createSSAOPipeline();

	void _generateKernelSamples();
};

#endif