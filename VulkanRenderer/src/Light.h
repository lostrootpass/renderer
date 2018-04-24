#ifndef LIGHT_H_
#define LIGHT_H_

#include <glm/glm.hpp>

struct Light
{
	glm::mat4 proj;
	glm::mat4 views[6];
	glm::vec4 color;
	glm::vec3 pos;
	uint32_t numViews;
	float farPlane;
};

#endif //LIGHT_H_