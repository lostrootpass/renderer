#ifndef MODEL_H_
#define MODEL_H_

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Texture.h"

class VulkanImpl;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 uv;
};

class Model
{
public:
	Model(const std::string& name, VulkanImpl* renderer);
	~Model();

	void draw(VulkanImpl* renderer, VkCommandBuffer cmd);

	const std::string& name() const
	{
		return _name;
	}

	const VkImageView texView() const
	{
		return _texture->view();
	}

private:
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;

	std::string _name;

	glm::vec3 _position;

	VkPipeline _pipeline;
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vtxMemory;
	VkBuffer _indexBuffer;
	VkDeviceMemory _idxMemory;

	Texture* _texture;

	void _load(VulkanImpl* renderer);
	void _loadTexture(const std::string& path, VulkanImpl* renderer);
	void _loadModel(VulkanImpl* renderer);
};

#endif //MODEL_H_