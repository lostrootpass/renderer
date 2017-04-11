#ifndef FRAMEBUFFER_H_
#define FRAMEBUFFER_H_

#include <vulkan/vulkan.h>

struct Framebuffer
{
	VkFramebuffer framebuffer;

	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;

	VkImage depthImage;
	VkImageView depthView;
	VkDeviceMemory depthMemory;
};

#endif //FRAMEBUFFER_H_