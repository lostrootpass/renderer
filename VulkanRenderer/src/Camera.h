#ifndef CAMERA_H_
#define CAMERA_H_

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

struct Camera
{
	glm::vec3 eye;
	glm::vec3 lookAt;

	void move(const glm::vec3& moveBy)
	{
		eye += moveBy;
	}

	glm::mat4 viewMatrix() const
	{
		glm::mat4 viewMatrix;

		return viewMatrix;
	}

	glm::mat4 projectionViewMatrix() const
	{
		glm::mat4 projectionMatrix;

		return projectionMatrix * viewMatrix();
	}
};

#endif //CAMERA_H_