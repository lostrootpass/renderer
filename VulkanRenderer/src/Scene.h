#ifndef SCENE_H_
#define SCENE_H_

#include <vector>
#include "Renderer.h"
#include "Light.h"
#include "Camera.h"

#include <SDL_keyboard.h>

class Model;
class RenderPass;

class Scene
{
public:
	Scene(Renderer& renderer);
	~Scene();

	void addModel(const std::string& name, float scale = 1.0f);

	void draw(VkCommandBuffer cmd, RenderPass& pass) const;
	
	void drawGeom(VkCommandBuffer cmd, RenderPass& pass) const;

	void drawShadow(VkCommandBuffer cmd, RenderPass& pass) const;

	void keyDown(SDL_Keycode key);

	inline const std::vector<Model*>& models() const
	{
		return _models;
	}

	void mouseMove(int dx, int dy);

	void resize(uint32_t width, uint32_t height);

	void update(float dtime);

	inline VkExtent2D viewport() const
	{
		return { _camera->width(), _camera->height() };
	}

private:
	std::vector<Model*> _models;
	std::vector<Light> _lights;

	Renderer* _renderer;
	Camera* _camera;

	uint32_t _sceneFlags;

	void _init();

	void _reload();

	void _setLightPos(const glm::vec3& pos);
};

#endif //SCENE_H_