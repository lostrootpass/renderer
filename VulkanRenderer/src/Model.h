#ifndef MODEL_H_
#define MODEL_H_

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class VulkanImpl;

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
};

class Model
{
public:
	Model(const std::string& name, VulkanImpl* renderer);
	~Model();

	void draw(VkCommandBuffer cmd);

private:
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;

	std::string _name;
	VkPipeline _pipeline;
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vtxMemory;
	VkBuffer _indexBuffer;
	VkDeviceMemory _idxMemory;

	void _load(VulkanImpl* renderer);
};

#endif //MODEL_H_