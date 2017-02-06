#ifndef CAMERA_H_
#define CAMERA_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>

const float DEFAULT_FOV = 60.0f;
const float DEFAULT_CLIP_NEAR = 0.1f;
const float DEFAULT_CLIP_FAR = 100.0f;

class Camera
{
public:
	Camera(uint32_t viewportWidth, uint32_t viewportHeight) :
		_fov(DEFAULT_FOV), _nearClip(DEFAULT_CLIP_NEAR), _farClip(DEFAULT_CLIP_FAR)
	{
		updateViewport(viewportWidth, viewportHeight);
		_reset();
	}

	void lookAt(const glm::vec3& point)
	{
		//
	}

	void mouseMove(int dx, int dy);

	void move(const glm::vec3& moveBy);

	glm::mat4 projectionViewMatrix() const
	{
		glm::mat4 projectionMatrix = glm::perspective(glm::radians(_fov), _aspectRatio, _nearClip, _farClip);
		projectionMatrix[1][1] *= -1; //Vulkan's Y-axis points the opposite direction to OpenGL's.

		return projectionMatrix * viewMatrix();
	}

	void update(float dtime);

	void updateViewport(uint32_t viewportWidth, uint32_t viewportHeight)
	{
		_aspectRatio = (viewportWidth / (float)viewportHeight);
	}

	glm::mat4 viewMatrix() const
	{
		return _orientation * _translation;
	}

private:
	glm::mat4 _translation;
	glm::mat4 _orientation;
	glm::quat _rotation;

	float _fov;
	float _aspectRatio;
	float _nearClip;
	float _farClip;

	void _adjustView(float yaw = 0.0f, float pitch = 0.0f);

	void _reset()
	{
		//Reset translation to origin.
		_translation = glm::mat4();

		//Point the camera pointing along the positive X-axis.
		_rotation = glm::quat();
		_rotation = glm::rotate(_rotation, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		_rotation = glm::rotate(_rotation, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		_orientation = glm::mat4_cast(glm::normalize(_rotation));
	}
};

#endif //CAMERA_H_