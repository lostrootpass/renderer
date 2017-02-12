#ifndef SWAP_CHAIN_H_
#define SWAP_CHAIN_H_

#include "Renderer.h"
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
	SwapChain(Renderer& vkImpl);
	SwapChain& operator=(const SwapChain&) = delete;
	SwapChain(const SwapChain&) = delete;
	SwapChain(SwapChain&&) = delete;
	~SwapChain();

	void init(VkSurfaceKHR surface);

	void present();
	
	void resize(uint32_t width, uint32_t height);

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
	Renderer* _impl;

	VkSwapchainKHR _vkSwapchain;
	VkSurfaceKHR _surface;

	VkImage _depthImage;
	VkDeviceMemory _depthMemory;
	VkImageView _depthView;

	VkSemaphore _imageAvailableSemaphore;
	VkSemaphore _renderingFinishedSemaphore;

	SwapChainInfo _swapChainInfo;

	void _cleanup();
	void _createDepthBuffer();
	void _createFramebuffers();
	void _createImageViews();
	void _createSemaphores();
	void _createSwapChain();
	void _populateSwapChainInfo();
};

#endif //SWAP_CHAIN_H_