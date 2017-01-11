#ifndef LIGHT_H_
#define LIGHT_H_

#include <glm/glm.hpp>

struct Light
{
	glm::vec4 color;
	glm::vec3 pos;
};

#endif //LIGHT_H_