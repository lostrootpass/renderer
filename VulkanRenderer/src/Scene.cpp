#include "Scene.h"
#include "Camera.h"
#include "Model.h"

Scene::Scene(VulkanImpl& renderer) : _camera(nullptr), _renderer(&renderer)
{
	_init();
}

Scene::~Scene()
{
	vkDeviceWaitIdle(VulkanImpl::device());

	for (Model* model : _models)
	{
		delete model;
	}

	_models.clear();

	delete _camera;
}

void Scene::addModel(const std::string& name)
{
	Model* model = new Model(name, _renderer);
	_models.push_back(model);

	//We've changed the scene and need to update the command buffers to reflect that.
	_renderer->recordCommandBuffers(this);
}

void Scene::draw(VkCommandBuffer cmd) const
{
	for (Model* model : _models)
	{
		model->draw(_renderer, cmd);
	}
}

void Scene::update(float dtime)
{
	for (Model* model : _models)
	{
		model->update(_renderer, dtime);
	}
}

void Scene::_init()
{
	VkExtent2D extent = _renderer->extent();
	_camera = new Camera(extent.width, extent.height);
	glm::mat4 projView = _camera->projectionViewMatrix();
	_renderer->updateUniform("camera", (void*)&projView, sizeof(projView));
}