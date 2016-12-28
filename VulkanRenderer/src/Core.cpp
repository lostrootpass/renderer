#include "Core.h"
#include "Window.h"

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
