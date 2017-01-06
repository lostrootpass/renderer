#ifndef BUFFER_H_
#define BUFFER_H_

#include <vulkan/vulkan.h>

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;

	~Buffer();
};

#endif //BUFFER_H_