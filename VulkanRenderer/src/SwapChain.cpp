#include "SwapChain.h"
#include "renderpass/RenderPass.h"

//VK_USE_PLATFORM_WIN32_KHR inadvertently prevents us using std::numeric_limits::max
#undef max

SwapChain::SwapChain(Renderer& vkImpl)
	: _vkSwapchain(VK_NULL_HANDLE), _surface(VK_NULL_HANDLE), _impl(&vkImpl)
{
	_populateSwapChainInfo();
}

SwapChain::~SwapChain()
{
	_cleanup();

	if (_vkSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(Renderer::device(), _vkSwapchain, nullptr);
}

void SwapChain::init(VkSurfaceKHR surface)
{
	_surface = surface;

	_createSwapChain();
	_createImageViews();
	_createDepthBuffer();
	_createSemaphores();
}

void SwapChain::present()
{
	uint32_t idx;
	VkCheck(vkAcquireNextImageKHR(Renderer::device(), _vkSwapchain, std::numeric_limits<uint64_t>::max(), _imageAvailableSemaphore, VK_NULL_HANDLE, &idx));

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

	VkCheck(vkQueueSubmit(_impl->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signals;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &idx;

	VkCheck(vkQueuePresentKHR(_impl->presentQueue(), &presentInfo));
}

void SwapChain::resize(uint32_t width, uint32_t height)
{
	_populateSwapChainInfo();
	_cleanup();

	_createSwapChain();
	_createImageViews();
	_createDepthBuffer();
	_createFramebuffers();
	_createSemaphores();
}

void SwapChain::_cleanup()
{
	vkDestroySemaphore(Renderer::device(), _imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(Renderer::device(), _renderingFinishedSemaphore, nullptr);

	vkDestroyImageView(Renderer::device(), _depthView, nullptr);
	vkDestroyImage(Renderer::device(), _depthImage, nullptr);
	vkFreeMemory(Renderer::device(), _depthMemory, nullptr);

	for (VkFramebuffer fb : _framebuffers)
	{
		vkDestroyFramebuffer(Renderer::device(), fb, nullptr);
	}
	_framebuffers.clear();

	for (VkImageView view : _imageViews)
	{
		vkDestroyImageView(Renderer::device(), view, nullptr);
	}
	_imageViews.clear();
}

void SwapChain::_createDepthBuffer()
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.extent.width = _swapChainInfo.surfaceCapabilities.currentExtent.width;
	info.extent.height = _swapChainInfo.surfaceCapabilities.currentExtent.height;
	info.extent.depth = 1;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.format = VK_FORMAT_D32_SFLOAT;
	info.imageType = VK_IMAGE_TYPE_2D;

	VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &_depthImage));

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(Renderer::device(), _depthImage, &memReq);

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memReq.size;
	alloc.memoryTypeIndex = _impl->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &_depthMemory));
	VkCheck(vkBindImageMemory(Renderer::device(), _depthImage, _depthMemory, 0));

	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.format = VK_FORMAT_D32_SFLOAT;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.image = _depthImage;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	view.subresourceRange.levelCount = 1;
	
	VkCheck(vkCreateImageView(Renderer::device(), &view, nullptr, &_depthView));
}

void SwapChain::_createFramebuffers()
{
	VkExtent2D extent = _swapChainInfo.surfaceCapabilities.currentExtent;

	for (VkImageView view : _imageViews)
	{
		const VkImageView attachments[] = {
			view, _depthView
		};

		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.width = extent.width;
		info.height = extent.height;
		info.renderPass = _impl->getRenderPass(RenderPassType::SCENE)->renderPass();
		info.attachmentCount = 2;
		info.pAttachments = attachments;
		info.layers = 1;
		

		VkFramebuffer framebuffer;
		VkCheck(vkCreateFramebuffer(Renderer::device(), &info, nullptr, &framebuffer));

		_framebuffers.push_back(framebuffer);
	}
}

void SwapChain::_createImageViews()
{
	uint32_t imageCount;
	VkCheck(vkGetSwapchainImagesKHR(Renderer::device(), _vkSwapchain, &imageCount, nullptr));
	_images.resize(imageCount);
	VkCheck(vkGetSwapchainImagesKHR(Renderer::device(), _vkSwapchain, &imageCount, _images.data()));

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
		VkCheck(vkCreateImageView(Renderer::device(), &info, nullptr, &view));

		_imageViews.push_back(view);
	}
}

void SwapChain::_createSemaphores()
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	VkCheck(vkCreateSemaphore(Renderer::device(), &info, nullptr, &_imageAvailableSemaphore));
	VkCheck(vkCreateSemaphore(Renderer::device(), &info, nullptr, &_renderingFinishedSemaphore));
}

void SwapChain::_createSwapChain()
{
	//TODO: pass these in from Renderer.
	//uint32_t indices[] = { _graphicsQueue.index, _presentQueue.index };
	VkSurfaceCapabilitiesKHR caps = _swapChainInfo.surfaceCapabilities;
	VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	info.clipped = VK_TRUE;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	info.surface = _surface;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.minImageCount = caps.minImageCount;
	info.imageExtent.width = caps.currentExtent.width;
	info.imageExtent.height = caps.currentExtent.height;
	info.preTransform = caps.currentTransform;
	info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

	//if (indices[0] == indices[1])
	{
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	//else
	//{
	//	info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	//	info.queueFamilyIndexCount = 2;
	//	info.pQueueFamilyIndices = indices;
	//}

	if (_vkSwapchain != VK_NULL_HANDLE)
	{
		info.oldSwapchain = _vkSwapchain;
	}

	//Keep existing swap chain around until after the new one is created
	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
	VkCheck(vkCreateSwapchainKHR(Renderer::device(), &info, nullptr, &newSwapchain));

	if (_vkSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(Renderer::device(), _vkSwapchain, nullptr);
	}

	_vkSwapchain = newSwapchain;
}

void SwapChain::_populateSwapChainInfo()
{
	VkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_impl->physicalDevice(), _impl->surface(), &_swapChainInfo.surfaceCapabilities));

	uint32_t formatCount;
	VkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(_impl->physicalDevice(), _impl->surface(), &formatCount, nullptr));

	if (formatCount > 0)
	{
		_swapChainInfo.surfaceFormats.resize(formatCount);
		VkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(_impl->physicalDevice(), _impl->surface(), &formatCount, _swapChainInfo.surfaceFormats.data()));
	}

	uint32_t presentModeCount;
	VkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(_impl->physicalDevice(), _impl->surface(), &presentModeCount, nullptr));

	if (presentModeCount > 0)
	{
		_swapChainInfo.presentModes.resize(presentModeCount);
		VkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(_impl->physicalDevice(), _impl->surface(), &presentModeCount, _swapChainInfo.presentModes.data()));
	}
}