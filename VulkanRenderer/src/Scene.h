#ifndef SCENE_H_
#define SCENE_H_

#include <vector>
#include "VulkanImpl.h"
#include "Light.h"

struct Camera;
class Model;

class Scene
{
public:
	Scene(VulkanImpl& renderer);
	~Scene();

	void addModel(const std::string& name);

	void draw(VkCommandBuffer cmd) const;

	void update(float dtime);

private:
	std::vector<Model*> _models;
	std::vector<Light> _lights;

	VulkanImpl* _renderer;
	Camera* _camera;

	void _init();
};

#endif //SCENE_H_