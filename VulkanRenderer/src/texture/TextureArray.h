#ifndef TEXTURE_ARRAY_H_
#define TEXTURE_ARRAY_H_

#include "Texture.h"
#include "../Renderer.h"

class TextureArray : public Texture
{
public:
	TextureArray(const std::vector<std::string>& paths, Renderer* renderer,
		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY);
	~TextureArray();

protected:
	void _createImage(Renderer* renderer, VkImageCreateInfo& info) override;
	
private:
	std::vector<std::string> _paths;
};

#endif //TEXTURE_ARRAY_H_