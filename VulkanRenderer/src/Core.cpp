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

void Core::run(int argc, char** argv)
{
	_init();

	if (argc > 1) //argv[0] on win32 is exe path
	{
		const float scale = (argc > 2 ? strtof(argv[2], 0) : 1.0f);
		_scene->addModel(argv[1], scale);
	}

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
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_RESIZED)
			{
				_scene->resize(e.window.data1, e.window.data2);
				_renderer->recreateSwapChain(e.window.data1, e.window.data2);
				_renderer->recordCommandBuffers(_scene);
			}
			break;
		case SDL_MOUSEMOTION:
			//TODO: have mouse motion & input be handled better.
			_scene->mouseMove(e.motion.xrel, e.motion.yrel);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			SDL_SetRelativeMouseMode((SDL_bool)(e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT));
			break;
		case SDL_KEYDOWN:
			_scene->keyDown(e.key.keysym.sym);
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
