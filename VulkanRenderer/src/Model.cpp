#include "Model.h"
#include "VulkanImpl.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

	vkDestroyImageView(VulkanImpl::device(), _texView, nullptr);
	vkDestroyImage(VulkanImpl::device(), _texImage, nullptr);
	vkFreeMemory(VulkanImpl::device(), _texMemory, nullptr);
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
	_loadModel(renderer);

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

void Model::_loadTexture(VulkanImpl* renderer, const std::string& path)
{
	int width, height, channels;
	stbi_uc* tex = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!tex)
		return;

	VkDeviceSize size = width * height * channels;

	VkBufferCreateInfo buff = {};
	buff.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buff.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buff.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buff.size = size;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	renderer->createAndBindBuffer(buff, &stagingBuffer, &stagingMemory, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* dst;
	vkMapMemory(VulkanImpl::device(), stagingMemory, 0, size, 0, &dst);
	memcpy(dst, (void*)tex, size);
	vkUnmapMemory(VulkanImpl::device(), stagingMemory);

	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent = { (uint32_t)width, (uint32_t)height, 1 };
	info.arrayLayers = 1;
	info.mipLevels = 1;
	info.format = VK_FORMAT_R8G8B8A8_UNORM;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	if (vkCreateImage(VulkanImpl::device(), &info, nullptr, &_texImage) != VK_SUCCESS)
	{
		//
	}

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(VulkanImpl::device(), _texImage, &memReq);

	VkMemoryAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	alloc.allocationSize = size;

	vkAllocateMemory(VulkanImpl::device(), &alloc, nullptr, &_texMemory);
	vkBindImageMemory(VulkanImpl::device(), _texImage, _texMemory, 0);

	VkImageSubresourceRange range = {};
	range.layerCount = 1;
	range.levelCount = 1;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	renderer->setImageLayout(_texImage, info.format, info.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

	VkBufferImageCopy copy = {};
	copy.imageExtent = info.extent;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;

	//If this doesn't match what was provided above, vkCmdCopyBufferToImage fails with a misleading/unhelpful message.
	copy.imageSubresource.mipLevel = 0;

	VkCommandBuffer cmd = renderer->startOneShotCmdBuffer();
	vkCmdCopyBufferToImage(cmd, stagingBuffer, _texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	renderer->submitOneShotCmdBuffer(cmd);

	renderer->setImageLayout(_texImage, info.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

	vkDestroyBuffer(VulkanImpl::device(), stagingBuffer, nullptr);
	vkFreeMemory(VulkanImpl::device(), stagingMemory, nullptr);

	stbi_image_free(tex);


	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.image = _texImage;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = info.format;
	view.subresourceRange = range;

	vkCreateImageView(VulkanImpl::device(), &view, nullptr, &_texView);
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

			vtx.uv = {
				attrib.texcoords[2 * i.texcoord_index],
				attrib.texcoords[2 * i.texcoord_index + 1]
			};

			_vertices.push_back(vtx);
			_indices.push_back((uint32_t)_indices.size());
		}
	}

	if (materials.size())
	{
		char diffuse[128] = { '\0' };
		sprintf_s(diffuse, "%s%s", baseDir, materials[0].diffuse_texname.c_str());
		_loadTexture(renderer, diffuse);
	}
}