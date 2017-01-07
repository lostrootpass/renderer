#ifndef BUFFER_H_
#define BUFFER_H_

#include <vulkan/vulkan.h>

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;

	~Buffer();

	void copyData(void* data, size_t size) const;
};

#endif //BUFFER_H_