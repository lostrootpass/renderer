#include "SwapChain.h"

//VK_USE_PLATFORM_WIN32_KHR inadvertently prevents us using std::numeric_limits::max
#undef max

SwapChain::SwapChain(VulkanImpl& vkImpl)
	: _vkSwapchain(VK_NULL_HANDLE), _impl(&vkImpl), _device(_impl->device())
{
	_populateSwapChainInfo();
}

SwapChain::~SwapChain()
{
	vkDestroySemaphore(_device, _imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(_device, _renderingFinishedSemaphore, nullptr);

	for (VkFramebuffer fb : _framebuffers)
	{
		vkDestroyFramebuffer(_device, fb, nullptr);
	}

	for (VkImageView view : _imageViews)
	{
		vkDestroyImageView(_device, view, nullptr);
	}

	if (_vkSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(_device, _vkSwapchain, nullptr);
}

void SwapChain::init(VkSwapchainCreateInfoKHR& info)
{
	if (_vkSwapchain != VK_NULL_HANDLE)
	{
		info.oldSwapchain = _vkSwapchain;
	}

	//Keep existing swap chain around until after the new one is created
	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
	vkCreateSwapchainKHR(_device, &info, nullptr, &newSwapchain);

	if (_vkSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(_device, _vkSwapchain, nullptr);
	}

	_vkSwapchain = newSwapchain;

	_createImageViews();
	_createFramebuffers();
	_createSemaphores();
}

void SwapChain::present()
{
	uint32_t idx;
	VkResult res;
	res = vkAcquireNextImageKHR(_device, _vkSwapchain, std::numeric_limits<uint64_t>::max(), _imageAvailableSemaphore, VK_NULL_HANDLE, &idx);

	if (res != VK_SUCCESS)
	{
		//
	}

	VkSemaphore semaphores[] = { _imageAvailableSemaphore };
	VkSemaphore signals[] = { _renderingFinishedSemaphore };
	VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkCommandBuffer buffers[] = { _impl->commandBuffer((size_t)idx) };
	VkSwapchainKHR swapChains[] = { _vkSwapchain };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = semaphores;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signals;
	submitInfo.pWaitDstStageMask = stages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = buffers;

	res = vkQueueSubmit(_impl->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);

	if (res != VK_SUCCESS)
	{
		//
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signals;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &idx;

	res = vkQueuePresentKHR(_impl->presentQueue(), &presentInfo);

	if (res != VK_SUCCESS)
	{
		//
	}
}

void SwapChain::_createFramebuffers()
{
	VkExtent2D extent = _swapChainInfo.surfaceCapabilities.currentExtent;

	for (VkImageView view : _imageViews)
	{
		const VkImageView attachments[] = {
			view
		};

		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.width = extent.width;
		info.height = extent.height;
		info.renderPass = _impl->renderPass();
		info.attachmentCount = 1;
		info.pAttachments = attachments;
		info.layers = 1;
		

		VkFramebuffer framebuffer;
		if (vkCreateFramebuffer(_impl->device(), &info, nullptr, &framebuffer) != VK_SUCCESS)
		{
			//
		}

		_framebuffers.push_back(framebuffer);
	}
}

void SwapChain::_createImageViews()
{
	uint32_t imageCount;
	vkGetSwapchainImagesKHR(_device, _vkSwapchain, &imageCount, nullptr);
	_images.resize(imageCount);
	vkGetSwapchainImagesKHR(_device, _vkSwapchain, &imageCount, _images.data());

	for (VkImage image : _images)
	{
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = image;
		info.format = VK_FORMAT_B8G8R8A8_UNORM;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;

		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;

		VkImageView view;
		if (vkCreateImageView(_device, &info, nullptr, &view) != VK_SUCCESS)
		{
			//
		}

		_imageViews.push_back(view);
	}
}

void SwapChain::_createSemaphores()
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	vkCreateSemaphore(_device, &info, nullptr, &_imageAvailableSemaphore);
	vkCreateSemaphore(_device, &info, nullptr, &_renderingFinishedSemaphore);
}

void SwapChain::_populateSwapChainInfo()
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_impl->physicalDevice(), _impl->surface(), &_swapChainInfo.surfaceCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(_impl->physicalDevice(), _impl->surface(), &formatCount, nullptr);

	if (formatCount > 0)
	{
		_swapChainInfo.surfaceFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(_impl->physicalDevice(), _impl->surface(), &formatCount, _swapChainInfo.surfaceFormats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(_impl->physicalDevice(), _impl->surface(), &presentModeCount, nullptr);

	if (presentModeCount > 0)
	{
		_swapChainInfo.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(_impl->physicalDevice(), _impl->surface(), &presentModeCount, _swapChainInfo.presentModes.data());
	}
}