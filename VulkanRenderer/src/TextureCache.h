#ifndef TEXTURE_CACHE_H_
#define TEXTURE_CACHE_H_

#include <unordered_map>

#include "VulkanImpl.h"
#include "Texture.h"

struct TextureCache final
{
	TextureCache& operator=(const TextureCache&) = delete;
	TextureCache(const TextureCache&) = delete;
	TextureCache(TextureCache&&) = delete;

	static void init()
	{

	}

	static Texture* const getTexture(const std::string& texturePath, VulkanImpl& renderer)
	{
		if (_textureCache.find(texturePath) != _textureCache.end())
			return _textureCache[texturePath];

		_loadTexture(texturePath, renderer);

		return _textureCache[texturePath];
	}

	static void shutdown()
	{
		for (auto pair : _textureCache)
		{
			delete pair.second;
		}

		_textureCache.clear();
	}

private:
	static std::unordered_map<std::string, Texture*> _textureCache;

	static void _loadTexture(const std::string& name, VulkanImpl& renderer)
	{
		_textureCache[name] = new Texture(name, &renderer);
	}
};

#endif //TEXTURE_CACHE_H_