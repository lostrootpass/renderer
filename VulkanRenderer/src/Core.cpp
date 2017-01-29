#include "Core.h"
#include "Window.h"
#include "Scene.h"

#include <iostream>
#include <chrono>

#include <SDL.h>

typedef std::chrono::high_resolution_clock Clock;

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

	std::chrono::time_point<std::chrono::steady_clock> now = Clock::now();
	std::chrono::duration<float> dtime;

	while (_running) {
		dtime = Clock::now() - now;
		now = Clock::now();

		_pollEvents();
		_scene->update(dtime.count());
		_renderer->render();

		Sleep(10);
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
