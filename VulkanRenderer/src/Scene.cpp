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
	_renderer->bindDescriptorSetById(cmd, SET_BINDING_LIGHTS, nullptr);
	_renderer->bindDescriptorSetById(cmd, SET_BINDING_CAMERA, nullptr);

	for (Model* model : _models)
	{
		model->draw(_renderer, cmd);
	}
}

void Scene::drawShadow(VkCommandBuffer cmd) const
{
	_renderer->bindDescriptorSetById(cmd, SET_BINDING_LIGHTS, nullptr, true);
	_renderer->bindDescriptorSetById(cmd, SET_BINDING_CAMERA, nullptr, true);

	for (Model* model : _models)
	{
		model->drawShadow(_renderer, cmd);
	}
}

void Scene::update(float dtime)
{
	_camera->update(dtime);
	glm::mat4 projView = _camera->projectionViewMatrix();
	_renderer->updateUniform("camera", (void*)&projView, sizeof(projView));

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


	//Test data

	addModel("cube");
	addModel("cube2");
	addModel("ground");
	_models[0]->setPosition(glm::vec3(0.0f, -1.5f, 1.0f));
	_models[0]->setScale(2.0f);
	_models[1]->setPosition(glm::vec3(0.0f, 1.5f, 1.0f));


	glm::vec3 eye = glm::vec3(8.0f, 8.0f, 8.0f);
	glm::mat4 mvp = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 100.0f);
	mvp[1][1] *= -1; //Vulkan's Y-axis points the opposite direction to OpenGL's.

	glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	mvp *= view;

	Light light;
	light.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	light.pos = eye;
	light.mvp = mvp;

	_lights.push_back(light);
	_renderer->updateUniform("light", (void*)&light, sizeof(light));
}