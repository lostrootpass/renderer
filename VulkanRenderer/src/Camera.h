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
};

#endif //CAMERA_H_