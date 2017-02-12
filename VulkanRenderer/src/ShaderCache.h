#ifndef SHADER_CACHE_H_
#define SHADER_CACHE_H_

#include "Renderer.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <fstream>

const std::string SHADER_EXT = ".spv";

struct ShaderCache final
{
	ShaderCache& operator=(const ShaderCache&) = delete;
	ShaderCache(const ShaderCache&) = delete;
	ShaderCache(ShaderCache&&) = delete;

	static void init()
	{
		
	}

	static VkShaderModule& getModule(const std::string& shaderName)
	{
		if (_moduleCache.find(shaderName) != _moduleCache.end())
			return _moduleCache[shaderName];

		_loadModule(shaderName);

		return _moduleCache[shaderName];
	}

	static void clear()
	{
		for (auto pair : _moduleCache)
		{
			vkDestroyShaderModule(Renderer::device(), pair.second, nullptr);
		}

		_moduleCache.clear();
	}

private:
	static std::unordered_map<std::string, VkShaderModule> _moduleCache;

	static void _loadModule(const std::string& name)
	{
		std::ifstream file(ASSET_PATH + name + SHADER_EXT, std::ios::binary | std::ios::in | std::ios::ate);
		size_t size = file.tellg();

		char* code = new char[size];
		file.seekg(0);
		file.read(code, size);

		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = size;
		info.pCode = (uint32_t*)code;

		VkShaderModule module;
		VkCheck(vkCreateShaderModule(Renderer::device(), &info, nullptr, &module));

		_moduleCache[name] = module;
		delete[] code;
	}
};

#endif