#include "Camera.h"

#include <SDL_keyboard.h>

void Camera::update(float dtime)
{
	const uint8_t* keys = SDL_GetKeyboardState(nullptr);

	if (keys[SDL_SCANCODE_R])
	{
		_reset();
		return;
	}

	//Position
	glm::vec3 velocity;

	const float BASE_VELOCITY = 1.5f;

	if (keys[SDL_SCANCODE_W])
		velocity.x = BASE_VELOCITY;
	else if(keys[SDL_SCANCODE_S])
		velocity.x = -BASE_VELOCITY;

	if (keys[SDL_SCANCODE_A])
		velocity.y = BASE_VELOCITY;
	else if (keys[SDL_SCANCODE_D])
		velocity.y = -BASE_VELOCITY;

	if (keys[SDL_SCANCODE_Q])
		velocity.z = BASE_VELOCITY;
	else if (keys[SDL_SCANCODE_E])
		velocity.z = -BASE_VELOCITY;

	if (keys[SDL_SCANCODE_LCTRL])
		velocity *= 0.5f;
	else if (keys[SDL_SCANCODE_LSHIFT])
		velocity *= 2.0f;

	move(velocity * dtime);
}