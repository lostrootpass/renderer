#ifndef WINDOW_H_
#define WINDOW_H_

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
	SDL_Window* _sdlWindow;
};

#endif //WINDOW_H_