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

	inline const VkImageView view() const
	{
		return _view;
	}

private:
	std::string _path;

	VkImage _image;
	VkImageView _view;
	VkDeviceMemory _memory;

	void _load(VulkanImpl* renderer);
};

#endif //TEXTURE_H_