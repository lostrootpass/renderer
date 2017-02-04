#include "Window.h"
#include "Core.h"

#include <SDL_syswm.h>

const int WIDTH = 800;
const int HEIGHT = 600;
const char* TITLE = "Vulkan Renderer";

Window::Window(Core& core) : _core(&core), _sdlWindow(nullptr)
{

}

Window::~Window()
{
	if (_sdlWindow != nullptr)
		close();
}

void Window::close()
{
	SDL_DestroyWindow(_sdlWindow);
	_sdlWindow = nullptr;
}

void Window::createSurface(const VkInstance vkInstance, VkSurfaceKHR* vkSurface) const
{
	SDL_SysWMinfo wmInfo = {};
	SDL_GetVersion(&wmInfo.version);
	SDL_GetWindowWMInfo(_sdlWindow, &wmInfo);
	
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hinstance = GetModuleHandle(NULL);
	createInfo.hwnd = wmInfo.info.win.window;
	
	VkCheck(vkCreateWin32SurfaceKHR(vkInstance, &createInfo, nullptr, vkSurface));
}

void Window::open()
{
	_sdlWindow = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE);
}
