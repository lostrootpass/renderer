#ifndef CORE_H_
#define CORE_H_

#include "Window.h"
#include "VulkanImpl.h"

class Scene;

class Core
{
public:
	Core();
	~Core();

	void run();

private:
	Renderer* _renderer;
	Window* _window;
	Scene* _scene;

	bool _running;

	void _init();
	void _pollEvents();
	void _shutdown();
};

#endif // CORE_H_