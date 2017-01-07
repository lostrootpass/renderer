#ifndef SCENE_H_
#define SCENE_H_

#include <vector>
#include "VulkanImpl.h"

struct Camera;
class Model;

class Scene
{
public:
	Scene(VulkanImpl& renderer);
	~Scene();

	void addModel(const std::string& name);

	void draw(VkCommandBuffer cmd) const;

private:
	std::vector<Model*> _models;

	VulkanImpl* _renderer;
	Camera* _camera;

	void _init();
};

#endif //SCENE_H_