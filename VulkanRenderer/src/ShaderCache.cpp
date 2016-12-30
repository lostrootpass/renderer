#include "ShaderCache.h"

std::unordered_map<std::string, VkShaderModule> ShaderCache::_moduleCache;