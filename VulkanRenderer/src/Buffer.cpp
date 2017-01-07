#include "Buffer.h"
#include "VulkanImpl.h"

Buffer::~Buffer()
{
	vkDestroyBuffer(VulkanImpl::device(), buffer, nullptr);
	vkFreeMemory(VulkanImpl::device(), memory, nullptr);
}

void Buffer::copyData(void* data, size_t size) const
{
	void* dst;
	vkMapMemory(VulkanImpl::device(), memory, 0, size, 0, &dst);
	memcpy(dst, data, size);
	vkUnmapMemory(VulkanImpl::device(), memory);
}