#include "ShadowMap.h"
#include "Scene.h"

const int SHADOW_MAP_WIDTH = 1024;
const int SHADOW_MAP_HEIGHT = 1024;
const VkFormat SHADOW_MAP_FORMAT = VK_FORMAT_D32_SFLOAT;

ShadowMap::ShadowMap(VulkanImpl& renderer)
{
	_init(&renderer);
}

ShadowMap::~ShadowMap()
{
	const VkDevice d = VulkanImpl::device();

	vkDestroyRenderPass(d, _renderPass, nullptr);
	vkDestroyFramebuffer(d, _framebuffer, nullptr);

	vkDestroyImageView(d, _depthView, nullptr);
	vkDestroyImage(d, _depthImage, nullptr);
	vkFreeMemory(d, _depthMemory, nullptr);
}

void ShadowMap::render(VkCommandBuffer cmd, const Scene* scene) const
{
	VkClearValue clear = { 1.0f, 0.0f };
	VkRenderPassBeginInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	info.clearValueCount = 1;
	info.pClearValues = &clear;
	info.renderPass = _renderPass;
	info.renderArea.offset = { 0, 0 };
	info.renderArea.extent = { SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT };
	info.framebuffer = _framebuffer;

	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

	if (scene)
		scene->drawShadow(cmd);

	vkCmdEndRenderPass(cmd);

	//_renderer->bindDescriptorSet(cmd, SET_BINDING_TEXTURE, _set);
}

void ShadowMap::_createDepthBuffer(VulkanImpl* renderer)
{
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.extent = { SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, 1 };
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.format = SHADOW_MAP_FORMAT;
	info.imageType = VK_IMAGE_TYPE_2D;

	VkCheck(vkCreateImage(VulkanImpl::device(), &info, nullptr, &_depthImage));

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(VulkanImpl::device(), _depthImage, &memReq);

	VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkCheck(vkAllocateMemory(VulkanImpl::device(), &alloc, nullptr, &_depthMemory));
	VkCheck(vkBindImageMemory(VulkanImpl::device(), _depthImage, _depthMemory, 0));

	VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view.format = SHADOW_MAP_FORMAT;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.image = _depthImage;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	view.subresourceRange.levelCount = 1;

	VkCheck(vkCreateImageView(VulkanImpl::device(), &view, nullptr, &_depthView));
}

void ShadowMap::_createFramebuffer()
{
	VkFramebufferCreateInfo info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	info.renderPass = _renderPass;
	info.width = SHADOW_MAP_WIDTH;
	info.height = SHADOW_MAP_HEIGHT;
	info.layers = 1;
	info.attachmentCount = 1;
	info.pAttachments = &_depthView;

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
	_renderer = renderer;

	_createRenderPass();
	_createDepthBuffer(renderer);
	_createFramebuffer();

	renderer->setOffscreenPass(_renderPass);


	//

	{
		renderer->allocateTextureDescriptor(_set, SET_BINDING_SHADOW);

		VkDescriptorImageInfo info = {};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = _depthView;

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.dstSet = _set;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.pImageInfo = &info;

		vkUpdateDescriptorSets(VulkanImpl::device(), 1, &write, 0, nullptr);
	}
}