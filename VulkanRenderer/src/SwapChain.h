#ifndef SWAP_CHAIN_H_
#define SWAP_CHAIN_H_

#include "VulkanImpl.h"
#include <vector>

struct SwapChainInfo
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain
{
public:
	SwapChain(VulkanImpl& vkImpl);
	SwapChain& operator=(const SwapChain&) = delete;
	SwapChain(const SwapChain&) = delete;
	SwapChain(SwapChain&&) = delete;
	~SwapChain();

	void init(VkSwapchainCreateInfoKHR& info);
	void present();

	inline const VkSurfaceCapabilitiesKHR& surfaceCapabilities() const
	{
		return _swapChainInfo.surfaceCapabilities;
	}

	inline const std::vector<VkSurfaceFormatKHR>& surfaceFormats() const
	{
		return _swapChainInfo.surfaceFormats;
	}

	inline const std::vector<VkPresentModeKHR>& presentModes() const
	{
		return _swapChainInfo.presentModes;
	}

	inline const std::vector<VkFramebuffer>& framebuffers() const
	{
		return _framebuffers;
	}

private:
	std::vector<VkImage> _images;
	std::vector<VkImageView> _imageViews;
	std::vector<VkFramebuffer> _framebuffers;
	VulkanImpl* _impl;

	VkSwapchainKHR _vkSwapchain;
	VkDevice _device;

	VkImage _depthImage;
	VkDeviceMemory _depthMemory;
	VkImageView _depthView;

	VkSemaphore _imageAvailableSemaphore;
	VkSemaphore _renderingFinishedSemaphore;

	SwapChainInfo _swapChainInfo;

	void _createDepthBuffer();
	void _createFramebuffers();
	void _createImageViews();
	void _createSemaphores();
	void _populateSwapChainInfo();
};

#endif //SWAP_CHAIN_H_