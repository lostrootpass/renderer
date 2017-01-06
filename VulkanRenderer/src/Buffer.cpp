#include "Buffer.h"
#include "VulkanImpl.h"

Buffer::~Buffer()
{
	vkDestroyBuffer(VulkanImpl::device(), buffer, nullptr);
	vkFreeMemory(VulkanImpl::device(), memory, nullptr);
}