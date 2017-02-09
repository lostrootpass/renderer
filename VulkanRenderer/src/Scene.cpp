#include "Scene.h"
#include "Camera.h"
#include "Model.h"

//TODO: move these?
enum SceneFlags
{
	SCENEFLAG_ENABLESHADOWS = 0x0001,
	SCENEFLAG_PRELIT = 0x0002,
	SCENEFLAG_ENABLEBUMPMAPS = 0x0004,
	SCENEFLAG_MAPSPLIT = 0x0008,
	SCENEFLAG_SHOWNORMALS = 0x0010,
	SCENEFLAG_ENABLESPECMAPS = 0x0020
};

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

void Scene::addModel(const std::string& name, float scale)
{
	Model* model = new Model(name, _renderer);
	model->setScale(scale);
	_models.push_back(model);
	
	//We've changed the scene and need to update the command buffers to reflect that.
	_renderer->recordCommandBuffers(this);
}

void Scene::draw(VkCommandBuffer cmd) const
{
	_renderer->updatePushConstants(cmd, sizeof(uint32_t), (void*)&_sceneFlags);

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

void Scene::keyDown(SDL_Keycode key)
{
	uint32_t flags = _sceneFlags;

	switch (key)
	{
	case SDLK_p:
		_sceneFlags ^= SCENEFLAG_PRELIT;
		break;
	case SDLK_b:
		_sceneFlags ^= SCENEFLAG_ENABLEBUMPMAPS;
		break;
	case SDLK_F2:
		_sceneFlags ^= SCENEFLAG_ENABLESHADOWS;
		break;
	case SDLK_m:
		_sceneFlags ^= SCENEFLAG_MAPSPLIT;
		break;
	case SDLK_n:
		_sceneFlags ^= SCENEFLAG_SHOWNORMALS;
		break;
	case SDLK_F1:
		_sceneFlags ^= SCENEFLAG_ENABLESPECMAPS;
		break;
	//A hacky way of getting the light to move to a specific position. TODO: fix.
	case SDLK_l:
		_lights[0].pos = _camera->eye();
		_lights[0].mvp = _camera->projectionViewMatrix();
		_renderer->updateUniform("light", (void*)&_lights[0], sizeof(_lights[0]));
		break;
	case SDLK_F5:
		_reload();
		break;
	}

	if(flags != _sceneFlags)
		_renderer->recordCommandBuffers(this);
}

void Scene::mouseMove(int dx, int dy)
{
	_camera->mouseMove(dx, dy);
}

void Scene::resize(uint32_t width, uint32_t height)
{
	_camera->updateViewport(width, height);
}

void Scene::update(float dtime)
{
	_camera->update(dtime);
	CameraUniform camera = { _camera->projectionViewMatrix(), _camera->eye() };
	_renderer->updateUniform("camera", (void*)&camera, sizeof(camera));
	//_setLightPos(_lights[0].pos + (glm::vec3(-1.0f * dtime, 0.0f, 0.0f)));

	for (Model* model : _models)
	{
		model->update(_renderer, dtime);
	}
}

void Scene::_init()
{
	VkExtent2D extent = _renderer->extent();
	_camera = new Camera(extent.width, extent.height);
	CameraUniform camera = { _camera->projectionViewMatrix(), _camera->eye() };
	_renderer->updateUniform("camera", (void*)&camera, sizeof(camera));

	Light light;
	light.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	_lights.push_back(light);
	_setLightPos(glm::vec3(-8.0f, 4.0f, 2.0f));

	_sceneFlags = SCENEFLAG_ENABLEBUMPMAPS | SCENEFLAG_ENABLESHADOWS | SCENEFLAG_ENABLESPECMAPS;
}

void Scene::_reload()
{
	_renderer->clearShaderCache();
	_renderer->destroyPipelines();

	for (Model* m : _models)
	{
		m->reload(_renderer);
	}

	_renderer->recordCommandBuffers(this);
}

void Scene::_setLightPos(const glm::vec3& pos)
{
	glm::mat4 mvp = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 100.0f);
	mvp[1][1] *= -1; //Vulkan's Y-axis points the opposite direction to OpenGL's.

	glm::mat4 view = glm::lookAt(pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mvp *= view;

	Light& light = _lights[0];
	light.pos = pos;
	light.mvp = mvp;
	_renderer->updateUniform("light", (void*)&light, sizeof(light));
}