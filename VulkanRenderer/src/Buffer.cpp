#include "Buffer.h"
#include "VulkanImpl.h"

Buffer::~Buffer()
{
	vkDestroyBuffer(VulkanImpl::device(), buffer, nullptr);
	vkFreeMemory(VulkanImpl::device(), memory, nullptr);
}

void Buffer::copyData(void* data, size_t size, size_t offset) const
{
	void* dst;
	vkMapMemory(VulkanImpl::device(), memory, offset, size, 0, &dst);
	memcpy(dst, data, size);
	vkUnmapMemory(VulkanImpl::device(), memory);
}