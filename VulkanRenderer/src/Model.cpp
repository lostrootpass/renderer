#include "Model.h"
#include "VulkanImpl.h"
#include "texture/TextureCache.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/gtc/matrix_transform.hpp>

static uint32_t MODEL_INDEX = 0;

Model::Model(const std::string& name, VulkanImpl* renderer)
	: _name(name), _position(glm::vec3(0.0f, 0.0f, 0.0f)), _scale(1.0f)
{
	_load(renderer);
	_index = MODEL_INDEX;
	MODEL_INDEX++;
}

Model::~Model()
{
	for (TextureArray* a : _materials)
		delete a;
}

void Model::draw(VulkanImpl* renderer, VkCommandBuffer cmd)
{
	renderer->bindDescriptorSetById(cmd, SET_BINDING_SAMPLER);
	
	std::vector<uint32_t> descOffsets = {(uint32_t)renderer->getAlignedRange(sizeof(ModelUniform))*_index};
	renderer->bindDescriptorSetById(cmd, SET_BINDING_MODEL, &descOffsets);

	std::vector<uint32_t> matOffsets = {(uint32_t)renderer->getAlignedRange(sizeof(MaterialData))*_index};
	renderer->bindDescriptorSetById(cmd, SET_BINDING_MATERIAL, &matOffsets);

	for (TextureArray* m : _materials)
	{
		if(m && m->set())
			renderer->bindDescriptorSet(cmd, SET_BINDING_TEXTURE, m->set());
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

	//TODO: copy whatever is in the Staging Buffer to GPU local memory
	
	for (const Shape& s : _shapes)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, &s.vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(cmd, s.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, (uint32_t)s.indices.size(), 1, 0, 0, 0);
	}
}

void Model::drawShadow(VulkanImpl* renderer, VkCommandBuffer cmd)
{
	std::vector<uint32_t> descOffsets = { (uint32_t)renderer->getAlignedRange(sizeof(ModelUniform))*_index };
	renderer->bindDescriptorSetById(cmd, SET_BINDING_MODEL, &descOffsets, true);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline);

	for (const Shape& s : _shapes)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, &s.vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(cmd, s.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, (uint32_t)s.indices.size(), 1, 0, 0, 0);
	}
}

void Model::reload(VulkanImpl* renderer)
{
	_pipeline = renderer->getPipelineForShader("shaders/common/model");
	_shadowPipeline = renderer->getPipelineForShader("shaders/common/shadowmap", true);
}

void Model::update(VulkanImpl* renderer, float dtime)
{
	static float time = 0;
	time += dtime;
	ModelUniform model = { glm::translate(glm::mat4(), _position), _scale };
	model.pos = glm::rotate(model.pos, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	model.pos = glm::rotate(model.pos, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	//model.pos = glm::rotate(model.pos, glm::radians(-90.0f) * (time/2.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	//TODO: Don't update the GPU-local memory here, just the staging buffer.
	renderer->updateUniform("model", (void*)&model, sizeof(model), renderer->getAlignedRange(sizeof(model)) * _index);
	renderer->updateUniform("material", (void*)&_materialData, sizeof(_materialData), renderer->getAlignedRange(sizeof(_materialData)) * _index);
}

void Model::_load(VulkanImpl* renderer)
{
	assert(sizeof(MaterialData) <= renderer->properties().limits.maxUniformBufferRange);

	_loadModel(renderer);

	//TODO: override default shaders if custom shaders are present in the model dir
	//char shaderName[128] = {'\0'};
	//sprintf_s(shaderName, "models/%s/%s", _name.c_str(), _name.c_str());
	//_pipeline = renderer->getPipelineForShader(shaderName);
	_pipeline = renderer->getPipelineForShader("shaders/common/model");
	_shadowPipeline = renderer->getPipelineForShader("shaders/common/shadowmap", true);

	//TODO: fix allocation inefficiencies - use one big buffer instead of lots of small ones.
	for (Shape& s : _shapes)
	{
		//Vertex buffer
		{
			size_t size = (s.vertices.size() * sizeof(Vertex));
			VkBufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.size = size;

			Buffer staging;
			renderer->createAndBindBuffer(info, staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			staging.copyData((void*)s.vertices.data(), size);

			info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			renderer->createAndBindBuffer(info, s.vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			renderer->copyBuffer(s.vertexBuffer, staging, size);
		}

		//Index buffer
		{
			size_t size = (s.indices.size() * sizeof(uint32_t));
			VkBufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.size = size;

			Buffer staging;
			renderer->createAndBindBuffer(info, staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			staging.copyData((void*)s.indices.data(), size);

			info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			renderer->createAndBindBuffer(info, s.indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			renderer->copyBuffer(s.indexBuffer, staging, size);
		}
	}
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

	glm::vec3* normals = nullptr;
	std::vector<glm::vec3>* faceNormals = nullptr;
	//attrib.normals.clear();

	//TODO: this approach is quite slow and could be optimised.
	if (!attrib.normals.size())
	{
		normals = new glm::vec3[attrib.vertices.size()];
		faceNormals = new std::vector<glm::vec3>[attrib.vertices.size()];

		const bool perVertexNormals = true;

		//First pass: calculate face normals.
		for (const tinyobj::shape_t& shape : shapes)
		{
			for (size_t idx = 0; idx < shape.mesh.indices.size(); idx += 3)
			{
				tinyobj::index_t i = shape.mesh.indices[idx];
				glm::vec3 vtx1(
					attrib.vertices[3 * i.vertex_index + 0],
					attrib.vertices[3 * i.vertex_index + 1],
					attrib.vertices[3 * i.vertex_index + 2]
				);

				tinyobj::index_t j = shape.mesh.indices[idx + 1];
				glm::vec3 vtx2(
					attrib.vertices[3 * j.vertex_index + 0],
					attrib.vertices[3 * j.vertex_index + 1],
					attrib.vertices[3 * j.vertex_index + 2]
				);

				tinyobj::index_t k = shape.mesh.indices[idx + 2];
				glm::vec3 vtx3(
					attrib.vertices[3 * k.vertex_index + 0],
					attrib.vertices[3 * k.vertex_index + 1],
					attrib.vertices[3 * k.vertex_index + 2]
				);

				glm::vec3 coplanarVec1 = vtx2 - vtx1;
				glm::vec3 coplanarVec2 = vtx3 - vtx1;
				glm::vec3 normal = glm::normalize(glm::cross(coplanarVec1, coplanarVec2));

				if (!perVertexNormals)
				{
					normals[i.vertex_index] = normal;
					normals[j.vertex_index] = normal;
					normals[k.vertex_index] = normal;
				}
				else
				{
					faceNormals[i.vertex_index].push_back(normal);
					faceNormals[j.vertex_index].push_back(normal);
					faceNormals[k.vertex_index].push_back(normal);
				}
			}
		}

		if (perVertexNormals)
		{
			//Second pass: calculate vertex normals.
			for (const tinyobj::shape_t& shape : shapes)
			{
				for (size_t idx = 0; idx < shape.mesh.indices.size(); idx++)
				{
					tinyobj::index_t i = shape.mesh.indices[idx];
					
					glm::vec3 sum(0.0f, 0.0f, 0.0f);

					for (glm::vec3 v : faceNormals[i.vertex_index])
						sum += v;

					normals[i.vertex_index] = glm::normalize(sum);
				}
			}
		}
	}


	_shapes.resize(shapes.size());
	_materials.resize(materials.size());

	
	for(size_t s = 0; s < shapes.size(); ++s)
	{
		const tinyobj::shape_t& shape = shapes[s];

		_shapes[s].name = shape.name;

		for(size_t idx = 0; idx < shape.mesh.indices.size(); idx++)
		{
			tinyobj::index_t i = shape.mesh.indices[idx];

			Vertex vtx = {};
			vtx.materialId = shape.mesh.material_ids[idx / 3];

			vtx.position = {
				attrib.vertices[3 * i.vertex_index] * scale,
				attrib.vertices[3 * i.vertex_index + 1] * scale,
				attrib.vertices[3 * i.vertex_index + 2] * scale,
			};

			if (attrib.texcoords.size())
			{
				vtx.uv = {
					attrib.texcoords[2 * i.texcoord_index],
					1.0 - attrib.texcoords[2 * i.texcoord_index + 1]
				};

				//assert(vtx.uv.x >= 0.0f && vtx.uv.x <= 1.0f);
				//assert(vtx.uv.y >= 0.0f && vtx.uv.y <= 1.0f);
			}

			if (attrib.normals.size())
			{
				vtx.normal = {
					attrib.normals[3 * i.normal_index],
					attrib.normals[3 * i.normal_index + 1],
					attrib.normals[3 * i.normal_index + 2],
				};
			}
			else
			{
				vtx.normal = normals[i.vertex_index];
			}

			//TODO: these can be cleaned up after being copied to the GPU
			_shapes[s].vertices.push_back(vtx);
			_shapes[s].indices.push_back((uint32_t)_shapes[s].indices.size());
		}
	}

	for(size_t i = 0; i < materials.size(); ++i)
	{
		tinyobj::material_t mat = materials[i];
		char texname[128] = { '\0' };

		_materialData.ambient[i] = { mat.ambient[0], mat.ambient[1], mat.ambient[2], 1.0 };
		_materialData.diffuse[i] = { mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0 };
		_materialData.specular[i] = { mat.specular[0], mat.specular[1], mat.specular[2], 1.0 };
		_materialData.emissive[i] = { mat.emission[0], mat.emission[1], mat.emission[2], 1.0 };
		_materialData.transparency[i] = { mat.transmittance[0], mat.transmittance[1], mat.transmittance[2], 1.0 };
		_materialData.shininess[i][0] = mat.shininess;
		_materialData.flags[i][0] = 0;

		std::vector<std::string> paths;

		if (mat.diffuse_texname != "")
		{
			sprintf_s(texname, "%s%s", baseDir, mat.diffuse_texname.c_str());
			paths.push_back(texname);
			_materialData.flags[i][0] |= MATFLAG_DIFFUSEMAP;
		}
		else
			paths.push_back("");

		if (mat.bump_texname != "")
		{
			sprintf_s(texname, "%s%s", baseDir, mat.bump_texname.c_str());
			paths.push_back(texname);
			_materialData.flags[i][0] |= MATFLAG_BUMPMAP;
		}
		else
			paths.push_back("");

		if (mat.specular_texname != "")
		{
			sprintf_s(texname, "%s%s", baseDir, mat.specular_texname.c_str());
			paths.push_back(texname);
			_materialData.flags[i][0] |= MATFLAG_SPECMAP;
		}
		else
			paths.push_back("");

		if (_materialData.flags[i][0] != 0)
		{
			//TODO: it's possible a texture array already exists for this material
			// could locate & reuse instead of reallocating
			_materials[i] = new TextureArray(paths, renderer);
			if (!_materialSet) _materialSet = &(_materials[i]->set());
			_materials[i]->load(renderer);
			_materials[i]->bind(renderer, *_materialSet, 0, (uint32_t)i);
		}
		else
			_materials[i] = nullptr;
	}


	//TODO: the validation layer warning that arises from not doing this is basically safe to ignore
	//however in the interest of keeping the output clean we do this here. Maybe there is a better way?
	for (size_t i = 0; i < 64; i++)
	{
		if((i >= _materials.size() || _materials[i] == nullptr) && _materials[0])
			_materials[0]->unbind(renderer, *_materialSet, 0, (uint32_t)i);
	}

	delete[] normals;
	delete[] faceNormals;
}