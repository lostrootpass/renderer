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

class Renderer;

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
	Model(const std::string& name, Renderer* renderer);
	~Model();

	void draw(Renderer* renderer, VkCommandBuffer cmd, RenderPass& pass);

	void drawGeom(Renderer* renderer, VkCommandBuffer cmd, RenderPass& pass);

	void drawShadow(Renderer* renderer, VkCommandBuffer cmd, RenderPass& pass);

	void reload(Renderer* renderer);

	void update(Renderer*, float dtime);

	inline const std::string& name() const
	{
		return _name;
	}

	inline VkDescriptorSet set() const
	{
		if (_materialSet)
			return *_materialSet;
		else
			return VK_NULL_HANDLE;
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
	VkPipeline _geomPipeline;
	const VkDescriptorSet* _materialSet;
	
	uint32_t _index;

	float _scale;

	void _load(Renderer* renderer);
	void _loadModel(Renderer* renderer);
};

#endif //MODEL_H_