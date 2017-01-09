#include "Scene.h"
#include "Camera.h"
#include "Model.h"

Scene::Scene(VulkanImpl& renderer) : _camera(nullptr), _renderer(&renderer)
{
	_init();


	//Test data - two distinct models at different positions.
	addModel("cube");
	addModel("cube2");
	_models[0]->setPosition(glm::vec3(-1.5f, 0.0f, 0.0f));
	_models[1]->setPosition(glm::vec3(1.5f, 0.0f, 0.0f));
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
	_renderer->bindDescriptorSetById(cmd, SET_BINDING_CAMERA, nullptr);

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
	_camera->eye = glm::vec3(0.0f, -4.0f, 2.0f);
	glm::mat4 projView = _camera->projectionViewMatrix();
	_renderer->updateUniform("camera", (void*)&projView, sizeof(projView));
}