#ifndef TEXTURE_ARRAY_H_
#define TEXTURE_ARRAY_H_

#include "Texture.h"
#include "../VulkanImpl.h"

class TextureArray : public Texture
{
public:
	TextureArray(const std::vector<std::string>& paths, VulkanImpl* renderer);
	~TextureArray();

protected:
	void _createImage(VulkanImpl* renderer, VkImageCreateInfo& info) override;
	
private:
	std::vector<std::string> _paths;
};

#endif //TEXTURE_ARRAY_H_