#include "ShadowMap.h"
#include "Scene.h"
#include "texture/Texture.h"

const uint32_t SHADOW_DIM = 1024;
const VkFormat SHADOW_MAP_FORMAT = VK_FORMAT_D32_SFLOAT;

ShadowMap::ShadowMap(VulkanImpl& renderer)
{
	_init(&renderer);
}

ShadowMap::~ShadowMap()
{
	vkDestroyRenderPass(VulkanImpl::device(), _renderPass, nullptr);
	vkDestroyFramebuffer(VulkanImpl::device(), _framebuffer, nullptr);

	delete _depthTexture;
}

void ShadowMap::render(VkCommandBuffer cmd, const Scene* scene) const
{
	VkClearValue clear = { 1.0f, 0.0f };
	VkRenderPassBeginInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	info.clearValueCount = 1;
	info.pClearValues = &clear;
	info.renderPass = _renderPass;
	info.renderArea.offset = { 0, 0 };
	info.renderArea.extent = { SHADOW_DIM, SHADOW_DIM };
	info.framebuffer = _framebuffer;

	VkViewport viewport = { 0, 0, (float)SHADOW_DIM, (float)SHADOW_DIM, 0.0f, 1.0f };
	VkRect2D scissor = { 0, 0, SHADOW_DIM, SHADOW_DIM };

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

	if (scene)
		scene->drawShadow(cmd);

	vkCmdEndRenderPass(cmd);

	//_renderer->bindDescriptorSet(cmd, SET_BINDING_TEXTURE, _set);
}

void ShadowMap::_createFramebuffer()
{
	VkImageView attachments[] = {
		_depthTexture->view()
	};
	VkFramebufferCreateInfo info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	info.renderPass = _renderPass;
	info.width = SHADOW_DIM;
	info.height = SHADOW_DIM;
	info.layers = 1;
	info.attachmentCount = 1;
	info.pAttachments = attachments;

	VkCheck(vkCreateFramebuffer(VulkanImpl::device(), &info, nullptr, &_framebuffer));
}

void ShadowMap::_createRenderPass()
{
	VkAttachmentReference attach = {};
	attach.attachment = 0;
	attach.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &attach;

	VkAttachmentDescription desc = {};
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	desc.format = SHADOW_MAP_FORMAT;
	
	VkRenderPassCreateInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.attachmentCount = 1;
	info.pAttachments = &desc;

	VkCheck(vkCreateRenderPass(VulkanImpl::device(), &info, nullptr, &_renderPass));
}

void ShadowMap::_init(VulkanImpl* renderer)
{
	_depthTexture = new Texture(SHADOW_DIM, SHADOW_DIM, SHADOW_MAP_FORMAT, renderer);
	_depthTexture->bind(renderer, 0);

	_createRenderPass();
	_createFramebuffer();

	renderer->setOffscreenPass(_renderPass);
}