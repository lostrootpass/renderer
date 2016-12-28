#ifndef WINDOW_H_
#define WINDOW_H_

//#define GLFW_INCLUDE_VULKAN
//#include <GLFW/glfw3.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <SDL_video.h>

class Core;

class Window
{
public:
	Window(Core& core);
	~Window();

	void close();
	void createSurface(const VkInstance vkInstance, VkSurfaceKHR* vkSurface) const;
	void open();

private:
	Core* _core;
	//GLFWwindow* _glfwWindow;
	SDL_Window* _sdlWindow;
};

#endif //WINDOW_H_