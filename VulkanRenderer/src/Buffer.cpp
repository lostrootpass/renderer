#include "Buffer.h"
#include "VulkanImpl.h"

Buffer::~Buffer()
{
	if(buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(VulkanImpl::device(), buffer, nullptr);

	if(memory != VK_NULL_HANDLE)
		vkFreeMemory(VulkanImpl::device(), memory, nullptr);
}

void Buffer::copyData(void* data, size_t size, size_t offset) const
{
	void* dst;
	VkCheck(vkMapMemory(VulkanImpl::device(), memory, offset, size, 0, &dst));
	memcpy(dst, data, size);
	vkUnmapMemory(VulkanImpl::device(), memory);
}