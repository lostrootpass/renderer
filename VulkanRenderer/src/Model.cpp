#include "Model.h"
#include "VulkanImpl.h"
#include "TextureCache.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/gtc/matrix_transform.hpp>

static uint32_t MODEL_INDEX = 0;

Model::Model(const std::string& name, VulkanImpl* renderer) : _name(name), _position(glm::vec3(0.0f, 0.0f, 0.0f))
{
	_load(renderer);
	_index = MODEL_INDEX;
	MODEL_INDEX++;
}

Model::~Model()
{
}

void Model::draw(VulkanImpl* renderer, VkCommandBuffer cmd)
{
	renderer->bindDescriptorSetById(cmd, SET_BINDING_SAMPLER);
	
	std::vector<uint32_t> descOffsets = {(uint32_t)renderer->getAlignedRange(sizeof(glm::mat4))*_index};
	renderer->bindDescriptorSetById(cmd, SET_BINDING_MODEL, &descOffsets);

	if(_texture && _texture->view())
		renderer->bindDescriptorSet(cmd, SET_BINDING_TEXTURE, _texture->set());
	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

	//TODO: copy whatever is in the Staging Buffer to GPU local memory
	
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &_vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, _indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, (uint32_t)_indices.size(), 1, 0, 0, 0);
}

void Model::update(VulkanImpl* renderer, float dtime)
{
	static float time = 0;
	time += dtime;
	glm::mat4 model = glm::translate(glm::mat4(), _position);
	//model = glm::rotate(model, time, glm::vec3(0.0f, 0.0f, 1.0f));

	//TODO: Don't update the GPU-local memory here, just the staging buffer.
	renderer->updateUniform("model", (void*)&model, sizeof(model), renderer->getAlignedRange(sizeof(glm::mat4)) * _index);
}

void Model::_load(VulkanImpl* renderer)
{
	_loadModel(renderer);

	//TODO: override default shaders if custom shaders are present in the model dir
	//char shaderName[128] = {'\0'};
	//sprintf_s(shaderName, "models/%s/%s", _name.c_str(), _name.c_str());
	//_pipeline = renderer->getPipelineForShader(shaderName);
	_pipeline = renderer->getPipelineForShader("shaders/common/model");

	//Vertex buffer
	{
		size_t size = (_vertices.size() * sizeof(Vertex));
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.size = size;

		Buffer staging;
		renderer->createAndBindBuffer(info, staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		staging.copyData((void*)_vertices.data(), size);

		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		renderer->createAndBindBuffer(info, _vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		renderer->copyBuffer(_vertexBuffer, staging, size);
	}

	//Index buffer
	{
		size_t size = (_indices.size() * sizeof(uint32_t));
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.size = size;

		Buffer staging;
		renderer->createAndBindBuffer(info, staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		staging.copyData((void*)_indices.data(), size);

		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		renderer->createAndBindBuffer(info, _indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		renderer->copyBuffer(_indexBuffer, staging, size);
	}
}

void Model::_loadTexture(const std::string& path, VulkanImpl* renderer)
{
	_texture = TextureCache::getTexture(path, *renderer);
}

void Model::_loadModel(VulkanImpl* renderer)
{
	char baseDir[128] = { '\0' };
	sprintf_s(baseDir, "assets/models/%s/", _name.c_str());

	char modelName[128] = { '\0' };
	sprintf_s(modelName, "%s%s.obj", baseDir, _name.c_str());

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelName, baseDir))
		return;
	
	const float scale = 1.0f;

	for (const tinyobj::shape_t& shape : shapes)
	{
		for (tinyobj::index_t i : shape.mesh.indices)
		{
			Vertex vtx = {};
			vtx.color = { 1.0f, 1.0f, 1.0f };

			vtx.position = {
				attrib.vertices[3 * i.vertex_index] * scale,
				attrib.vertices[3 * i.vertex_index + 1] * scale,
				attrib.vertices[3 * i.vertex_index + 2] * scale,
			};

			if (attrib.texcoords.size())
			{
				vtx.uv = {
					attrib.texcoords[2 * i.texcoord_index],
					attrib.texcoords[2 * i.texcoord_index + 1]
				};
			}

			_vertices.push_back(vtx);
			_indices.push_back((uint32_t)_indices.size());
		}
	}

	if (materials.size())
	{
		char diffuse[128] = { '\0' };
		sprintf_s(diffuse, "%s%s", baseDir, materials[0].diffuse_texname.c_str());
		_loadTexture(diffuse, renderer);
	}
}