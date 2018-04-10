#ifndef TEXTURE_CACHE_H_
#define TEXTURE_CACHE_H_

#include <unordered_map>

#include "../Renderer.h"
#include "Texture.h"

struct TextureCache final
{
	TextureCache& operator=(const TextureCache&) = delete;
	TextureCache(const TextureCache&) = delete;
	TextureCache(TextureCache&&) = delete;

	static void init()
	{

	}

	static Texture* const getTexture(const std::string& texturePath, Renderer& renderer)
	{
		if (_textureCache.find(texturePath) != _textureCache.end())
			return _textureCache[texturePath];

		_loadTexture(texturePath, renderer);

		return _textureCache[texturePath];
	}

	static void shutdown()
	{
		for (TextureCachePair& pair : _textureCache)
		{
			delete pair.second;
		}

		_textureCache.clear();
	}

private:
	typedef std::pair<const std::string, Texture*> TextureCachePair;
	static std::unordered_map<std::string, Texture*> _textureCache;

	static void _loadTexture(const std::string& name, Renderer& renderer)
	{
		_textureCache[name] = new Texture(name, &renderer);
	}
};

#endif //TEXTURE_CACHE_H_