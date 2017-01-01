#include "Model.h"
#include "VulkanImpl.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

Model::Model(const std::string& name, VulkanImpl* renderer) : _name(name), _position(glm::vec3(0.0f, 0.0f, 0.0f))
{
	_load(renderer);
}

Model::~Model()
{
	vkDestroyBuffer(VulkanImpl::device(), _vertexBuffer, nullptr);
	vkDestroyBuffer(VulkanImpl::device(), _indexBuffer, nullptr);
	vkFreeMemory(VulkanImpl::device(), _vtxMemory, nullptr);
	vkFreeMemory(VulkanImpl::device(), _idxMemory, nullptr);
}

void Model::draw(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &_vertexBuffer, offsets);
	vkCmdBindIndexBuffer(cmd, _indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, (uint32_t)_indices.size(), 1, 0, 0, 0);
}

void Model::_load(VulkanImpl* renderer)
{
	_loadModel();

	char shaderName[128] = {'\0'};
	sprintf_s(shaderName, "models/%s/%s", _name.c_str(), _name.c_str());
	_pipeline = renderer->getPipelineForShader(shaderName);

	//Vertex buffer
	{
		size_t size = (_vertices.size() * sizeof(Vertex));
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.size = size;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		renderer->createAndBindBuffer(info, &stagingBuffer, &stagingMemory, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* dst;
		vkMapMemory(VulkanImpl::device(), stagingMemory, 0, size, 0, &dst);
		memcpy(dst, (void*)_vertices.data(), size);
		vkUnmapMemory(VulkanImpl::device(), stagingMemory);

		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		renderer->createAndBindBuffer(info, &_vertexBuffer, &_vtxMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		renderer->copyBuffer(&_vertexBuffer, &stagingBuffer, size);

		vkDestroyBuffer(VulkanImpl::device(), stagingBuffer, nullptr);
		vkFreeMemory(VulkanImpl::device(), stagingMemory, nullptr);
	}

	//Index buffer
	{
		size_t size = (_indices.size() * sizeof(uint32_t));
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.size = size;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		renderer->createAndBindBuffer(info, &stagingBuffer, &stagingMemory, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* dst;
		vkMapMemory(VulkanImpl::device(), stagingMemory, 0, size, 0, &dst);
		memcpy(dst, (void*)_indices.data(), size);
		vkUnmapMemory(VulkanImpl::device(), stagingMemory);

		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		renderer->createAndBindBuffer(info, &_indexBuffer, &_idxMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		renderer->copyBuffer(&_indexBuffer, &stagingBuffer, size);

		vkDestroyBuffer(VulkanImpl::device(), stagingBuffer, nullptr);
		vkFreeMemory(VulkanImpl::device(), stagingMemory, nullptr);
	}
}

void Model::_loadModel()
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

			_vertices.push_back(vtx);
			_indices.push_back((uint32_t)_indices.size());
		}
	}
}