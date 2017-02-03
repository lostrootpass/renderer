#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <vulkan/vulkan.h>
#include <string>

#include "SetBinding.h"

class VulkanImpl;

class Texture
{
public:
	Texture(const std::string& path, VulkanImpl* renderer);

	Texture(uint32_t width, uint32_t height, VkFormat format, VulkanImpl* renderer);

	~Texture();

	void bind(VulkanImpl* renderer, VkDescriptorSet set = VK_NULL_HANDLE, uint32_t binding = 0);

	inline const VkDescriptorSet& set() const
	{
		return _set;
	}

	inline const VkImageView view() const
	{
		return _view;
	}

private:
	std::string _path;

	VkImage _image;
	VkImageView _view;
	VkDeviceMemory _memory;
	VkDescriptorSet _set;

	uint32_t _width;
	uint32_t _height;
	VkFormat _format;

	void _allocBindImageMemory(VulkanImpl* renderer);
	void _createInMemory(VulkanImpl* renderer);
	void _loadFromFile(VulkanImpl* renderer);
	void _updateSet(VulkanImpl* renderer, VkDescriptorSet set, uint32_t binding = 0);
};

#endif //TEXTURE_H_