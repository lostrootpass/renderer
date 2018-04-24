#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include "../Renderer.h"
#include "TextureCache.h"

Texture::Texture(const std::string& path, Renderer* renderer)
	: _path(path), _format(VK_FORMAT_R8G8B8A8_UNORM), _set(VK_NULL_HANDLE),
	_layers(1), _viewType(VK_IMAGE_VIEW_TYPE_2D), _width(0), _height(0)
{
	load(renderer);
}

Texture::Texture(uint32_t width, uint32_t height, VkFormat format, 
	VkImageViewType viewType, Renderer* renderer) 
	: _path(""), _width(width), _height(height), _format(format), _layers(1),
	_viewType(viewType)
{
	//TODO: HACK. move this to a different type of Texture.
	_createInMemory(renderer);
}

Texture::~Texture()
{
	for(VkImageView v : _views)
		vkDestroyImageView(Renderer::device(), v, nullptr);

	vkDestroyImage(Renderer::device(), _image, nullptr);
	vkFreeMemory(Renderer::device(), _memory, nullptr);
}

void Texture::bind(Renderer* renderer, VkDescriptorSet set, uint32_t binding,
	uint32_t index)
{
	//Take the set if provided, otherwise check/update internal set
	if (set == VK_NULL_HANDLE)
	{
		if (_set == VK_NULL_HANDLE)
			renderer->allocateTextureDescriptor(_set);

		_updateSet(renderer, _set, binding, index);
	}
	else
		_updateSet(renderer, set, binding, index);
}

bool Texture::load(Renderer* renderer)
{
	VkImageCreateInfo info = {};
	_createImage(renderer, info);

	assert(_image);
	if (!_image)
		return false;

	_allocBindImageMemory(renderer);

	VkImageSubresourceRange range = {};
	range.layerCount = _layers;
	range.levelCount = 1;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	renderer->setImageLayout(_image, _format, info.initialLayout, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

	std::vector<VkBufferImageCopy> copies;

	for (size_t i = 0; i < _extents.size(); ++i)
	{
		VkBufferImageCopy copy = {};
		copy.imageExtent = _extents[i];
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.baseArrayLayer = (uint32_t)i;
		copy.imageSubresource.layerCount = 1;
		copy.bufferOffset = i * (_width * _height * 4);
		copy.bufferImageHeight = _extents[i].height;
		copy.bufferRowLength = _extents[i].width;
		copy.imageSubresource.mipLevel = 0;

		copies.push_back(copy);
	}

	VkCommandBuffer cmd = renderer->startOneShotCmdBuffer();
	vkCmdCopyBufferToImage(cmd, _staging.buffer, _image, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copies.size(),
		copies.data());
	renderer->submitOneShotCmdBuffer(cmd);

	renderer->setImageLayout(_image, _format, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, 
		VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.image = _image;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	view.format = _format;
	view.subresourceRange = range;

	_views.resize(1);
	VkCheck(vkCreateImageView(Renderer::device(), &view, nullptr, &_views[0]));

	return true;
}

void Texture::unbind(Renderer* renderer, VkDescriptorSet set, uint32_t binding,
	uint32_t index)
{
	VkDescriptorImageInfo info = {};
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.imageView = TextureCache::getTexture("assets/textures/missingtexture.png",
		*renderer)->view();

	VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.dstSet = set;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(Renderer::device(), 1, &write, 0, nullptr);
}

void Texture::_allocBindImageMemory(Renderer* renderer)
{
	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(Renderer::device(), _image, &memReq);

	VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &_memory));
	VkCheck(vkBindImageMemory(Renderer::device(), _image, _memory, 0));
}

void Texture::_createImage(Renderer* renderer, VkImageCreateInfo& info)
{
	int channels;
	stbi_uc* tex = stbi_load(_path.c_str(), (int*)&_width, (int*)&_height,
		&channels, STBI_rgb_alpha);

	if (!tex)
		return;

	//Force an alpha channel to be allocated even if we don't need one.
	VkDeviceSize size = _width * _height * 4;

	VkExtent3D extent = { _width, _height, 1 };
	_extents.push_back(extent);

	VkBufferCreateInfo buff = {};
	buff.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buff.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buff.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buff.size = size * 1;

	renderer->createAndBindBuffer(buff, _staging, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	_staging.copyData((void*)tex, size);

	stbi_image_free(tex);

	info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent = extent;
	info.arrayLayers = 1;
	info.mipLevels = 1;
	info.format = _format;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &_image));
}

void Texture::_createInMemory(Renderer* renderer)
{
	VkExtent3D extent = { _width, _height, 1 };
	_extents.push_back(extent);

	const uint32_t layers = (_viewType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;

	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	if (layers == 6)
		info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	else
		info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.extent = { _width,_height, 1 };
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.mipLevels = 1;
	info.arrayLayers = layers;
	info.format = _format;
	info.imageType = VK_IMAGE_TYPE_2D;

	if (_viewType == VK_IMAGE_VIEW_TYPE_CUBE)
		info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &_image));

	_allocBindImageMemory(renderer);

	VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view.format = _format;
	view.viewType = _viewType;
	view.image = _image;

	if (layers == 6)
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	else
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	view.subresourceRange.levelCount = 1;

	_views.resize(layers);
	for (uint32_t i = 0; i < layers; ++i)
	{
		view.subresourceRange.baseArrayLayer = i;
		VkCheck(vkCreateImageView(Renderer::device(), &view, nullptr, &_views[i]));
	}

	renderer->allocateTextureDescriptor(_set, SET_BINDING_SHADOW);
}

void Texture::_updateSet(Renderer* renderer, VkDescriptorSet set, 
	uint32_t binding, uint32_t index)
{
	VkDescriptorImageInfo info = {};
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.imageView = _views[0];

	VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.dstSet = set;
	write.dstBinding = binding;
	write.dstArrayElement = index;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(Renderer::device(), 1, &write, 0, nullptr);
}