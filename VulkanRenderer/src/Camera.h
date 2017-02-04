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
		_lookAt = point;
	}

	void move(const glm::vec3& moveBy)
	{
		_eye += moveBy;
		lookAt(_lookAt + moveBy);
	}

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
		glm::mat4 viewMatrix = glm::lookAt(_eye, _lookAt, glm::vec3(0.0f, 0.0f, 1.0f));

		return viewMatrix;
	}

private:
	glm::vec3 _eye;
	glm::vec3 _lookAt;

	float _fov;
	float _aspectRatio;
	float _nearClip;
	float _farClip;

	void _reset()
	{
		_eye = glm::vec3(-8.0f, 0.0f, 2.0f);
		//_eye = glm::vec3(-3.0f, 0.0f, 0.0f);
		lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
	}
};

#endif //CAMERA_H_