#ifndef VULKAN_IMPL_UTIL_H_
#define VULKAN_IMPL_UTIL_H_

#include "VulkanImpl.h"
#include <vector>

const char* VK_VALIDATION_LAYERS[] = {
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME
};

struct VulkanImpl::Util
{
	static bool getRequiredExtensions(std::vector<const char*>& extensions)
	{

		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

		if (VulkanImpl::DEBUGENABLE)
		{
			for(size_t i = 0; i < ARRAYSIZE(VK_VALIDATION_LAYERS); i++)
				extensions.push_back(VK_VALIDATION_LAYERS[i]);
		}

		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData)
	{
		printf("Validation layer: %s\r\n", msg);

		return VK_FALSE;
	}
};

#endif //VULKAN_IMPL_UTIL_H_