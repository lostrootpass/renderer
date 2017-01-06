#ifndef CAMERA_H_
#define CAMERA_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
	glm::vec3 eye;
	glm::vec3 lookAt;

	float fov;
	float aspectRatio;
	float nearClip;
	float farClip;

	Camera(uint32_t viewportWidth, uint32_t viewportHeight) :
		eye(glm::vec3(3.0f, 3.0f, 3.0f)), lookAt(glm::vec3(0.0f, 0.0f, 0.0f)),
		fov(60.0f), nearClip(0.1f), farClip(100.0f)
	{
		updateViewport(viewportWidth, viewportHeight);
	}

	void move(const glm::vec3& moveBy)
	{
		eye += moveBy;
	}

	glm::mat4 projectionViewMatrix() const
	{
		glm::mat4 projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
		projectionMatrix[1][1] *= -1; //Vulkan's Y-axis points the opposite direction to OpenGL's.

		return projectionMatrix * viewMatrix();
	}

	void updateViewport(uint32_t viewportWidth, uint32_t viewportHeight)
	{
		aspectRatio = (viewportWidth / (float)viewportHeight);
	}

	glm::mat4 viewMatrix() const
	{
		glm::mat4 viewMatrix = glm::lookAt(eye, lookAt, glm::vec3(0.0f, 0.0f, 1.0f));

		return viewMatrix;
	}
};

#endif //CAMERA_H_