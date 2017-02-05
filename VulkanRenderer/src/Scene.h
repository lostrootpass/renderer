#ifndef SCENE_H_
#define SCENE_H_

#include <vector>
#include "VulkanImpl.h"
#include "Light.h"

class Camera;
class Model;

class Scene
{
public:
	Scene(VulkanImpl& renderer);
	~Scene();

	void addModel(const std::string& name, float scale = 1.0f);

	void draw(VkCommandBuffer cmd) const;

	void drawShadow(VkCommandBuffer cmd) const;

	void resize(uint32_t width, uint32_t height);

	void update(float dtime);

private:
	std::vector<Model*> _models;
	std::vector<Light> _lights;

	VulkanImpl* _renderer;
	Camera* _camera;

	void _init();

	void _setLightPos(const glm::vec3& pos);
};

#endif //SCENE_H_