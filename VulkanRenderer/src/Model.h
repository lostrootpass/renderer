#ifndef MODEL_H_
#define MODEL_H_

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Texture.h"
#include "Buffer.h"

class VulkanImpl;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 uv;
	glm::vec3 normal;
};

struct ModelUniform
{
	glm::mat4 pos;
	float scale;
};

class Model
{
public:
	Model(const std::string& name, VulkanImpl* renderer);
	~Model();

	void draw(VulkanImpl* renderer, VkCommandBuffer cmd);

	void drawShadow(VulkanImpl* renderer, VkCommandBuffer cmd);

	void update(VulkanImpl*, float dtime);

	inline const std::string& name() const
	{
		return _name;
	}

	inline void setPosition(glm::vec3 pos)
	{
		_position = pos;
	}

	inline void setScale(float scale)
	{
		_scale = glm::max(0.0f, scale);
	}

private:
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;

	std::string _name;

	glm::vec3 _position;

	VkPipeline _pipeline;
	VkPipeline _shadowPipeline;
	VkDescriptorSet _textureSet;
	
	Buffer _vertexBuffer;
	Buffer _indexBuffer;

	Texture* _diffuse;
	Texture* _bumpMap;

	uint32_t _index;

	float _scale;

	void _load(VulkanImpl* renderer);
	void _loadModel(VulkanImpl* renderer);
};

#endif //MODEL_H_