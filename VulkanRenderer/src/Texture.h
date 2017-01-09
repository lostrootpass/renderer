#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <vulkan/vulkan.h>
#include <string>

class VulkanImpl;

class Texture
{
public:
	Texture(const std::string& path, VulkanImpl* renderer);

	~Texture();

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

	void _load(VulkanImpl* renderer);
};

#endif //TEXTURE_H_