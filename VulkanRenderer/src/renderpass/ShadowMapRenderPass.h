#ifndef SHADOW_MAP_RENDER_PASS_H_
#define SHADOW_MAP_RENDER_PASS_H_

#include "RenderPass.h"

class Scene;

enum class ShadowMapType
{
	SHADOW_MAP_2D,
	SHADOW_MAP_CUBE
};

class ShadowMapRenderPass : public RenderPass
{
public:
	ShadowMapRenderPass(Scene& scene, ShadowMapType type) : _scene(&scene),
		_depthTexture(nullptr), _type(type) {}

	~ShadowMapRenderPass();

	virtual void init(Renderer* renderer) override;

	void recreateShadowMap(Renderer* renderer);

	virtual void render(VkCommandBuffer cmd, const Framebuffer*) override;

	inline VkDescriptorSet set() const
	{
		return _depthTexture ? _depthTexture->set() : VK_NULL_HANDLE;
	}

	inline virtual RenderPassType type() {
		return RenderPassType::SHADOWMAP;
	};

protected:
	virtual void _createDescriptorSets(Renderer* renderer) override;

	virtual void _createPipeline(const std::string& shaderName) override;

	virtual void _createPipelineLayout() override;

	virtual void _createRenderPass() override;

private:
	std::vector<VkFramebuffer> _framebuffers;

	Scene* _scene;

	Texture* _depthTexture;

	ShadowMapType _type;

	void _createFramebuffer();
};

#endif //SHADOW_MAP_RENDER_PASS_H_