#include "Buffer.h"
#include "Renderer.h"

Buffer::~Buffer()
{
	if(buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(Renderer::device(), buffer, nullptr);

	if(memory != VK_NULL_HANDLE)
		vkFreeMemory(Renderer::device(), memory, nullptr);
}

void Buffer::copyData(void* data, size_t size, size_t offset) const
{
	void* dst;
	VkCheck(vkMapMemory(Renderer::device(), memory, offset, size, 0, &dst));
	memcpy(dst, data, size);
	vkUnmapMemory(Renderer::device(), memory);
}