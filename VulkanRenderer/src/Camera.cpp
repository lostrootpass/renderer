#include "Camera.h"

#include <SDL_keyboard.h>
#include <SDL_mouse.h>

void Camera::mouseMove(int dx, int dy)
{
	int xPos, yPos;
	Uint32 state = SDL_GetMouseState(&xPos, &yPos);
	if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
	{
		//dx/dy come through to us in pixels, and obviously we don't want 1px = 1rad of rotation
		float moveScale = 0.004f;

		_adjustView(dx * moveScale, dy * moveScale);
	}
}

void Camera::move(const glm::vec3& moveBy)
{
	glm::vec3 r = { _orientation[0][0], _orientation[1][0], _orientation[2][0] };
	glm::vec3 u = { _orientation[0][1], _orientation[1][1], _orientation[2][1] };
	glm::vec3 f = { _orientation[0][2], _orientation[1][2], _orientation[2][2] };
	_translation = glm::translate(_translation, (-f * moveBy.x));
	_translation = glm::translate(_translation, (r * moveBy.y));

	if (true /*world_space_elevation*/)
	{
		//use the world 'up' vector. more natural for this use case.
		_translation = glm::translate(_translation, (glm::vec3(0.0f, 0.0f, 1.0f) * -moveBy.z));
	}
	else
	{
		//use camera-space to determine what "up" is - can be disorienting to the user.
		_translation = glm::translate(_translation, (-u * moveBy.z));
	}
}

void Camera::update(float dtime)
{
	const uint8_t* keys = SDL_GetKeyboardState(nullptr);

	if (keys[SDL_SCANCODE_R])
	{
		_reset();
		return;
	}

	if (keys[SDL_SCANCODE_L])
	{
		//TODO: look at the model in focus.
		lookAt(glm::vec3(0.0, 0.0, 0.0));
	}

	//Position
	glm::vec3 velocity(0.0f);

	const float BASE_VELOCITY = 1.5f;

	if (keys[SDL_SCANCODE_W])
		velocity.x = -BASE_VELOCITY;
	else if(keys[SDL_SCANCODE_S])
		velocity.x = BASE_VELOCITY;

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
		velocity *= 3.0f;

	move(velocity * dtime);

	float yaw = 0.0f;
	float pitch = 0.0f;

	//Rotation

	//TODO: this camera axis layout is objectively correct. However, some people insist on being wrong.
	//We should enable these people to be wrong by letting them change axis layouts in the settings.
	if (keys[SDL_SCANCODE_LEFT])
		yaw = -1.0f;
	else if (keys[SDL_SCANCODE_RIGHT])
		yaw = 1.0f;

	//TODO: clamp pitch to prevent camera inversion?
	if (keys[SDL_SCANCODE_UP])
		pitch = -1.0f;
	else if (keys[SDL_SCANCODE_DOWN])
		pitch = 1.0f;
	
	_adjustView(yaw * dtime, pitch * dtime);
}

void Camera::_adjustView(float yaw, float pitch)
{
	//Calculate the pitch in camera space, but...
	_rotation = glm::mix(glm::quat(), glm::quat(1.0f, pitch * 1.0f, 0.0f, 0.0f), 1.0f);

	// ... calculate the yaw in world space to avoid DO A BARREL ROLL.
	glm::mat4 yawMatrix = glm::rotate(glm::mat4(), glm::radians(90.0f) * yaw, glm::vec3(0.0f, 0.0f, 1.0f));

	_orientation = glm::mat4_cast(_rotation) * _orientation * yawMatrix;
}