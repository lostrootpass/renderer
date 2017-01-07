#include "Core.h"
#include "Window.h"
#include "Scene.h"

#include <iostream>

#include <SDL.h>

Core::Core() : _running(true), _window(nullptr), _renderer(nullptr)
{

}

Core::~Core()
{
	_shutdown();
}

void Core::run()
{
	_init();

	while (_running) {
		_pollEvents();
		_renderer->render();
	}

	_shutdown();
}

void Core::_init()
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	_window = new Window(*this);
	_window->open();

	_renderer = new Renderer();
	_renderer->init(*_window);

	_scene = new Scene(*_renderer);

	//Test model:
	_scene->addModel("cube");
}

void Core::_pollEvents()
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
		case SDL_QUIT:
			_running = false;
			break;
		}
	}
}

void Core::_shutdown()
{
	_running = false;

	if (_scene)
	{
		delete _scene;
		_scene = nullptr;
	}

	if (_renderer)
	{
		delete _renderer;
		_renderer = nullptr;
	}

	if (_window)
	{
		_window->close();
		delete _window;
		_window = nullptr;
	}

	SDL_Quit();
}
