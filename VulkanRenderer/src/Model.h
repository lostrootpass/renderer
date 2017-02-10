#ifndef MODEL_H_
#define MODEL_H_

#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "texture/Texture.h"
#include "texture/TextureArray.h"
#include "Buffer.h"

class VulkanImpl;

struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
	uint8_t materialId;
};

//Materials have to be done this way as the spec only enforces maxPerStageDescriptorUniformBuffers >= 12
//While AMD has appropriately high values for this setting, NVIDIA sticks to the minimum, thus the odd layout.
static const size_t INDICES = 64;
struct MaterialData
{
	glm::vec4 ambient[INDICES];
	glm::vec4 diffuse[INDICES];
	glm::vec4 specular[INDICES];
	glm::vec4 emissive[INDICES];
	glm::vec4 transparency[INDICES];
	uint32_t flags[INDICES][4];
	float shininess[INDICES][4];
};

enum MatFlags
{
	MATFLAG_DIFFUSEMAP = 0x0001,
	MATFLAG_BUMPMAP = 0x0002,
	MATFLAG_SPECMAP = 0x0004,
	MATFLAG_NORMALMAP = 0x0008,
	MATFLAG_PRELIT = 0x0010,
	MATFLAG_ALPHAMASK = 0x0020
};

struct Shape
{
	Buffer vertexBuffer;
	Buffer indexBuffer;

	std::string name;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
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

	void reload(VulkanImpl* renderer);

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
	std::vector<Shape> _shapes;
	std::vector<TextureArray*> _materials;
	MaterialData _materialData;

	std::string _name;

	glm::vec3 _position;

	VkPipeline _pipeline;
	VkPipeline _shadowPipeline;
	const VkDescriptorSet* _materialSet;
	
	uint32_t _index;

	float _scale;

	void _load(VulkanImpl* renderer);
	void _loadModel(VulkanImpl* renderer);
};

#endif //MODEL_H_