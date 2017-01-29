#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include "VulkanImpl.h"

Texture::Texture(const std::string& path, VulkanImpl* renderer) 
	: _path(path), _format(VK_FORMAT_R8G8B8A8_UNORM)
{
	_loadFromFile(renderer);
}

Texture::Texture(uint32_t width, uint32_t height, VkFormat format, VulkanImpl* renderer) 
	: _path(""), _width(width), _height(height), _format(format)
{
	_createInMemory(renderer);
}

Texture::~Texture()
{
	vkDestroyImageView(VulkanImpl::device(), _view, nullptr);
	vkDestroyImage(VulkanImpl::device(), _image, nullptr);
	vkFreeMemory(VulkanImpl::device(), _memory, nullptr);
}

void Texture::_allocBindImageMemory(VulkanImpl* renderer)
{
	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(VulkanImpl::device(), _image, &memReq);

	VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkCheck(vkAllocateMemory(VulkanImpl::device(), &alloc, nullptr, &_memory));
	VkCheck(vkBindImageMemory(VulkanImpl::device(), _image, _memory, 0));
}

void Texture::_createInMemory(VulkanImpl* renderer)
{
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.extent = { _width, _height, 1 };
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.format = _format;
	info.imageType = VK_IMAGE_TYPE_2D;

	VkCheck(vkCreateImage(VulkanImpl::device(), &info, nullptr, &_image));

	_allocBindImageMemory(renderer);

	VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view.format = _format;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.image = _image;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	view.subresourceRange.levelCount = 1;

	VkCheck(vkCreateImageView(VulkanImpl::device(), &view, nullptr, &_view));

	_updateSet(renderer, SET_BINDING_SHADOW, 0);
}

void Texture::_loadFromFile(VulkanImpl* renderer)
{
	int channels;
	stbi_uc* tex = stbi_load(_path.c_str(), (int*)&_width, (int*)&_height, &channels, STBI_rgb_alpha);

	if (!tex)
		return;

	//Force an alpha channel to be allocated even if we don't need one.
	VkDeviceSize size = _width * _height * 4;

	VkBufferCreateInfo buff = {};
	buff.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buff.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buff.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buff.size = size;

	Buffer staging;
	renderer->createAndBindBuffer(buff, staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	staging.copyData((void*)tex, size);

	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent = { _width, _height, 1 };
	info.arrayLayers = 1;
	info.mipLevels = 1;
	info.format = _format;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	VkCheck(vkCreateImage(VulkanImpl::device(), &info, nullptr, &_image));

	_allocBindImageMemory(renderer);

	VkImageSubresourceRange range = {};
	range.layerCount = 1;
	range.levelCount = 1;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	renderer->setImageLayout(_image, info.format, info.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

	VkBufferImageCopy copy = {};
	copy.imageExtent = info.extent;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;

	//If this doesn't match what was provided above, vkCmdCopyBufferToImage fails with a misleading/unhelpful message.
	copy.imageSubresource.mipLevel = 0;

	VkCommandBuffer cmd = renderer->startOneShotCmdBuffer();
	vkCmdCopyBufferToImage(cmd, staging.buffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	renderer->submitOneShotCmdBuffer(cmd);

	renderer->setImageLayout(_image, info.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

	stbi_image_free(tex);


	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.image = _image;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = info.format;
	view.subresourceRange = range;

	VkCheck(vkCreateImageView(VulkanImpl::device(), &view, nullptr, &_view));

	_updateSet(renderer);
}

void Texture::_updateSet(VulkanImpl* renderer, SetBinding set, uint32_t binding)
{
	renderer->allocateTextureDescriptor(_set, set);

	VkDescriptorImageInfo info = {};
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.imageView = _view;

	VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.dstSet = _set;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(VulkanImpl::device(), 1, &write, 0, nullptr);
}