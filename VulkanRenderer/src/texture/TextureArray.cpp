#include "TextureArray.h"

#include <stb_image.h>

TextureArray::TextureArray(const std::vector<std::string>& paths, Renderer* renderer)
	: _paths(paths), Texture((uint8_t)paths.size())
{
	assert(_layers > 0);
}

TextureArray::~TextureArray()
{

}

void TextureArray::_createImage(Renderer* renderer, VkImageCreateInfo& info)
{
	uint32_t width = 0, height = 0;
	int channels;
	
	VkDeviceSize totalSize = 0;
	
	for (std::string& p : _paths)
	{
		if (p == "")
			continue;

		//TODO: more graceful error handling.
		assert(stbi_info(p.c_str(), (int*)&width, (int*)&height, &channels));

		//TODO: fix.
		//Force an alpha channel to be allocated even if we don't need one.
		totalSize += (width * height * 4);

		if (width > _width)
			_width = width;

		if (height > _height)
			_height = height;
	}

	//TODO: if an image is < stride then we need to somehow communicate re-normalised UVs to the shader.
	VkDeviceSize stride = _width * _height * 4;
	
	VkBufferCreateInfo buff = {};
	buff.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buff.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buff.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buff.size = stride * _layers;

	renderer->createAndBindBuffer(buff, _staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	stbi_uc* tex;
	for (size_t i = 0; i < _paths.size(); ++i)
	{
		if (_paths[i] != "")
		{
			tex = stbi_load(_paths[i].c_str(), (int*)&width, (int*)&height, &channels, STBI_rgb_alpha);
			VkDeviceSize copySize = width * height * 4;
			_staging.copyData((void*)tex, copySize, stride * i);
			stbi_image_free(tex);
		}
		else
		{
			//Empty image; assume stride length;
			//TODO: could pack this tighter.
			width = _width;
			height = _height;
		}

		VkExtent3D extent = { width, height, 1 };
		_extents.push_back(extent);
	}

	info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent = { _width, _height, 1 };
	info.arrayLayers = _layers;
	info.mipLevels = 1;
	info.format = _format;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &_image));
}