#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "../SetBinding.h"
#include "../Buffer.h"

class VulkanImpl;

class Texture
{
public:
	Texture(const std::string& path, VulkanImpl* renderer);

	Texture(uint32_t width, uint32_t height, VkFormat format, VulkanImpl* renderer);

	~Texture();

	void bind(VulkanImpl* renderer, VkDescriptorSet set = VK_NULL_HANDLE, uint32_t binding = 0, uint32_t index = 0);

	void load(VulkanImpl* renderer);

	void unbind(VulkanImpl* renderer, VkDescriptorSet set = VK_NULL_HANDLE, uint32_t binding = 0, uint32_t index = 0);

	inline const VkDescriptorSet& set() const
	{
		return _set;
	}

	inline const VkImageView view() const
	{
		return _view;
	}

protected:
	Texture(uint8_t layers) 
		: _path(""), _layers(layers), _viewType(VK_IMAGE_VIEW_TYPE_2D_ARRAY),
		_format(VK_FORMAT_R8G8B8A8_UNORM)
	{};

	VkImage _image;
	VkImageView _view;
	VkDeviceMemory _memory;
	VkDescriptorSet _set;

	//Individual dimensions of each image used by this Texture
	std::vector<VkExtent3D> _extents;

	//Maximum dimensions (i.e. stride) of all images used by this Texture
	uint32_t _width;
	uint32_t _height;
	
	VkFormat _format;
	VkImageViewType _viewType;
	const uint8_t _layers;

	Buffer _staging;

	void _allocBindImageMemory(VulkanImpl* renderer);

	//TODO: make Texture abstract and rename existing Texture to Texture2D
	virtual void _createImage(VulkanImpl* renderer, VkImageCreateInfo& info) /*= 0*/;

	void _updateSet(VulkanImpl* renderer, VkDescriptorSet set, uint32_t binding = 0, uint32_t index = 0);

private:
	std::string _path;

	//TODO: move to DynamicTexture, DepthTexture, etc. or similar class
	void _createInMemory(VulkanImpl* renderer);
};

#endif //TEXTURE_H_