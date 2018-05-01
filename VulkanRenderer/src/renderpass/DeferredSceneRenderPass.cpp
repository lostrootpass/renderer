#include "DeferredSceneRenderPass.h"
#include "ShadowMapRenderPass.h"
#include "SSAORenderPass.h"
#include "../Scene.h"
#include "../Model.h"
#include "../ShaderCache.h"
#include "../Renderer.h"
#include "../SwapChain.h"

//TODO: retrieve from global config.
const uint32_t MAX_MATERIALS = 64;
const uint32_t MAX_TEXTURES = 64;
const uint32_t MAX_MODELS = 64;

DeferredSceneRenderPass::~DeferredSceneRenderPass()
{
	delete _ssaoPass;

	const VkDevice d = Renderer::device();
	vkDestroySampler(d, _sampler, nullptr);

	vkDestroyPipeline(d, _deferredPipeline, nullptr);
	vkDestroyPipelineLayout(d, _deferredPipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(d, _deferredSetLayout, nullptr);

	for (VkDescriptorSetLayout l : _deferredSetLayouts)
		vkDestroyDescriptorSetLayout(d, l, nullptr);

	_cleanupDeferredTargets();

	vkDestroyRenderPass(d, _geometryPass, nullptr);
}

void DeferredSceneRenderPass::allocateTextureDescriptor(VkDescriptorSet& set, SetBinding binding)
{
	VkDescriptorSetAllocateInfo alloc = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc.descriptorSetCount = 1;
	alloc.descriptorPool = _descriptorPool;
	alloc.pSetLayouts = &_descriptorLayouts[binding];
	VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, &set));
}

void DeferredSceneRenderPass::init(Renderer* renderer)
{
	_renderer = renderer;
	_extent = renderer->extent();

	_createRenderPass();
	_createPipelineLayout();
	_createDescriptorSets(renderer);

	_createDeferredLayout();
	_createDeferredPipeline();
	
	_ssaoPass = new SSAORenderPass();
	_ssaoPass->init(renderer);
}

void DeferredSceneRenderPass::reload()
{
	vkDeviceWaitIdle(Renderer::device());
	vkDestroyPipeline(Renderer::device(), _deferredPipeline, nullptr);
	_deferredPipeline = VK_NULL_HANDLE;
	_createDeferredPipeline();
}

void DeferredSceneRenderPass::render(VkCommandBuffer cmd, const Framebuffer* framebuffer)
{
	VkClearValue clearValues[] = {
		{ 0.0f, 0.0f, 0.2f, 1.0f }, //Clear color
		{ 0.2f, 0.0f, 0.0f, 1.0f }, //Normal color
		{ 1.0f, 0.0f } //Depth stencil
	};

	_extent = _scene->viewport();

	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.clearValueCount = 3;
	info.pClearValues = clearValues;
	info.renderPass = _geometryPass;
	info.renderArea.offset = { 0, 0 };
	info.renderArea.extent = _extent;


	VkDescriptorSet shadow = ((ShadowMapRenderPass*)_shadowPass)->set();

	//Geometry pass first
	info.framebuffer = _deferredFramebuffers[0].framebuffer;

	VkViewport viewport = { 
		0, 0, (float)_extent.width, (float)_extent.height, 0.0f, 1.0f
	};
	VkRect2D scissor = { 0, 0, _extent.width, _extent.height };

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

	bindDescriptorSet(cmd, SET_BINDING_SHADOW, shadow);

	if (_scene)
		_scene->drawGeom(cmd, *this);

	vkCmdEndRenderPass(cmd);


	//Convert RTs from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier memBarriers[3] = {};
	memBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memBarriers[0].image = _deferredFramebuffers[0].image;
	memBarriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	memBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	memBarriers[0].srcAccessMask = 0;
	memBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	memBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memBarriers[0].subresourceRange.layerCount = 1;
	memBarriers[0].subresourceRange.levelCount = 1;

	memBarriers[2] = memBarriers[1] = memBarriers[0];
	memBarriers[1].image = _deferredFramebuffers[0].normalImage;
	memBarriers[2].image = _deferredFramebuffers[0].depthImage;
	memBarriers[2].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	memBarriers[2].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
		0, nullptr, 0, nullptr, 3, memBarriers);


	//SSAO pass
	_ssaoPass->render(cmd);
	
	//Then do the shading pass
	VkClearValue clrValues[] = {
		{ 0.0f, 0.0f, 0.2f, 1.0f }, //Clear color
		{ 1.0f, 0.0f } //Depth stencil
	};
	info.framebuffer = framebuffer->framebuffer;
	info.clearValueCount = 2;
	info.pClearValues = clrValues;
	info.renderPass = _renderPass;
	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _deferredPipeline);

	VkDescriptorSet deferredSets[] = {
		_bindingSet, _descriptorSets[SET_BINDING_SAMPLER],
		_descriptorSets[SET_BINDING_LIGHTS],
		_scene->models()[0]->set(), //TODO: multiple model support
		_descriptorSets[SET_BINDING_CAMERA],
		shadow,
		_descriptorSets[SET_BINDING_MATERIAL]
	};

	uint32_t offsets[] = { 0 };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
		_deferredPipelineLayout, 0, 7, deferredSets, 1, offsets);

	vkCmdDraw(cmd, 4, 1, 0, 0);

	vkCmdEndRenderPass(cmd);
}

void DeferredSceneRenderPass::resize(uint32_t width, uint32_t height)
{
	_cleanupDeferredTargets();
	_ssaoPass->resize(width, height);
	_createRenderTargets(_renderer);
	_ssaoPass->setAttachmentBinding(_bindingSet);
}

void DeferredSceneRenderPass::_createDescriptorSets(Renderer* renderer)
{
	VkDescriptorPoolSize sizes[5] = {};
	//Camera matrix & lights
	sizes[0].descriptorCount = 2;
	sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	//Sampler
	sizes[1].descriptorCount = 1;
	sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;

	//Textures
	sizes[2].descriptorCount = MAX_TEXTURES * MAX_MATERIALS + 2;
	sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

	//Model & material data
	sizes[3].descriptorCount = 2 + MAX_MATERIALS;
	sizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

	//Combined samplers
	sizes[4].descriptorCount = MAX_TEXTURES;
	sizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool.poolSizeCount = 5;
	pool.pPoolSizes = sizes;
	pool.maxSets = SET_BINDING_COUNT*2 + MAX_TEXTURES*2;

	VkCheck(vkCreateDescriptorPool(Renderer::device(), &pool, nullptr, &_descriptorPool));

	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = _descriptorPool;
	alloc.pSetLayouts = _descriptorLayouts.data();
	alloc.descriptorSetCount = (uint32_t)_descriptorLayouts.size();

	_descriptorSets.resize(SET_BINDING_COUNT);

	VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, _descriptorSets.data()));

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;

	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16.0f;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;

	if(_sampler == VK_NULL_HANDLE)
		VkCheck(vkCreateSampler(Renderer::device(), &samplerInfo, nullptr, &_sampler));


	{
		VkDescriptorBufferInfo buff = {};
		Uniform* uniform = renderer->getUniform("camera");
		buff.buffer = uniform->localBuffer.buffer;
		buff.offset = 0;
		buff.range = uniform->size;

		VkWriteDescriptorSet writes[1] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstSet = _descriptorSets[SET_BINDING_CAMERA];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pBufferInfo = &buff;

		vkUpdateDescriptorSets(Renderer::device(), 1, writes, 0, nullptr);
	}

	{
		size_t range = renderer->getAlignedRange(sizeof(ModelUniform));

		VkDescriptorBufferInfo buff = {};
		Uniform* uniform = renderer->getUniform("model");
		buff.buffer = uniform->localBuffer.buffer;
		buff.offset = 0;
		buff.range = range;

		VkWriteDescriptorSet writes[2] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writes[0].dstSet = _descriptorSets[SET_BINDING_MODEL];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pBufferInfo = &buff;

		vkUpdateDescriptorSets(Renderer::device(), 1, writes, 0, nullptr);
	}

	{
		VkDescriptorImageInfo img = {};
		img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img.sampler = renderer->sampler();

		VkWriteDescriptorSet writes[1] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writes[0].dstSet = _descriptorSets[SET_BINDING_SAMPLER];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pImageInfo = &img;

		vkUpdateDescriptorSets(Renderer::device(), 1, writes, 0, nullptr);
	}

	{
		VkDescriptorBufferInfo buff = {};
		Uniform* uniform = renderer->getUniform("light");
		buff.buffer = uniform->localBuffer.buffer;
		buff.offset = 0;
		buff.range = uniform->size;

		VkWriteDescriptorSet writes[1] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].dstSet = _descriptorSets[SET_BINDING_LIGHTS];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pBufferInfo = &buff;

		vkUpdateDescriptorSets(Renderer::device(), 1, writes, 0, nullptr);
	}

	{
		size_t range = renderer->getAlignedRange(sizeof(MaterialData));

		VkDescriptorBufferInfo buff = {};
		Uniform* uniform = renderer->getUniform("material");
		buff.buffer = uniform->localBuffer.buffer;
		buff.offset = 0;
		buff.range = range;


		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.dstSet = _descriptorSets[SET_BINDING_MATERIAL];
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.pBufferInfo = &buff;

		vkUpdateDescriptorSets(Renderer::device(), 1, &write, 0, nullptr);
	}
}

void DeferredSceneRenderPass::_createPipeline(const std::string& shaderName)
{
	VkPipeline pipeline;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[0].module = ShaderCache::getModule(shaderName + ".vert");

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";
	stages[1].module = ShaderCache::getModule(shaderName + ".frag");

	VkPipelineColorBlendAttachmentState cba = {};
	cba.blendEnable = VK_TRUE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	cba.colorBlendOp = VK_BLEND_OP_ADD;
	cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	VkPipelineColorBlendAttachmentState blendAttachments[2] = { cba, cba };

	VkPipelineColorBlendStateCreateInfo cbs = {};
	cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbs.attachmentCount = 2;
	cbs.pAttachments = blendAttachments;
	cbs.logicOp = VK_LOGIC_OP_COPY;

	VkPipelineInputAssemblyStateCreateInfo ias = {};
	ias.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	ias.primitiveRestartEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo mss = {};
	mss.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	mss.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.lineWidth = 1.0f;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.depthBiasEnable = VK_TRUE;
	rs.depthBiasConstantFactor = 0.005f;
	rs.depthBiasSlopeFactor = 0.8f;

	VkVertexInputBindingDescription vbs = {};
	vbs.binding = 0;
	vbs.stride = sizeof(Vertex);
	vbs.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	const int VTX_ATTR_COUNT = 4;
	VkVertexInputAttributeDescription vtxAttrs[VTX_ATTR_COUNT] = {};
	vtxAttrs[0].binding = 0;
	vtxAttrs[0].location = 0;
	vtxAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vtxAttrs[0].offset = offsetof(Vertex, position);

	vtxAttrs[1].binding = 0;
	vtxAttrs[1].location = 1;
	vtxAttrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	vtxAttrs[1].offset = offsetof(Vertex, uv);

	vtxAttrs[2].binding = 0;
	vtxAttrs[2].location = 2;
	vtxAttrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	vtxAttrs[2].offset = offsetof(Vertex, normal);

	vtxAttrs[3].binding = 0;
	vtxAttrs[3].location = 3;
	vtxAttrs[3].format = VK_FORMAT_R8_UINT;
	vtxAttrs[3].offset = offsetof(Vertex, materialId);

	VkPipelineVertexInputStateCreateInfo vis = {};
	vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vis.vertexBindingDescriptionCount = 1;
	vis.pVertexBindingDescriptions = &vbs;
	vis.vertexAttributeDescriptionCount = VTX_ATTR_COUNT;
	vis.pVertexAttributeDescriptions = vtxAttrs;

	//Set dynamically.
	VkRect2D sc = {};
	VkViewport vp = {};

	VkPipelineViewportStateCreateInfo vps = {};
	vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vps.viewportCount = 1;
	vps.scissorCount = 1;
	vps.pViewports = &vp;
	vps.pScissors = &sc;

	VkPipelineDepthStencilStateCreateInfo dss = {};
	dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dss.depthTestEnable = VK_TRUE;
	dss.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	dss.depthWriteEnable = VK_TRUE;

	VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dys = {};
	dys.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dys.dynamicStateCount = 2;
	dys.pDynamicStates = dynStates;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.layout = _pipelineLayout;
	info.renderPass = _geometryPass;
	info.stageCount = 2;
	info.subpass = 0;
	info.pStages = stages;
	info.pColorBlendState = &cbs;
	info.pInputAssemblyState = &ias;
	info.pMultisampleState = &mss;
	info.pRasterizationState = &rs;
	info.pVertexInputState = &vis;
	info.pViewportState = &vps;
	info.pDepthStencilState = &dss;
	info.pDynamicState = &dys;

	VkCheck(vkCreateGraphicsPipelines(Renderer::device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

	_pipelines[shaderName] = pipeline;
}

void DeferredSceneRenderPass::_createPipelineLayout()
{
	VkDescriptorSetLayoutBinding bindings[2] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	bindings[0].descriptorCount = 1;

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 1;
	info.pBindings = bindings;

	_descriptorLayouts.resize(SET_BINDING_COUNT);

	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_CAMERA]));

	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_MODEL]));

	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].descriptorCount = 1;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_SAMPLER]));

	info.bindingCount = 1;
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].descriptorCount = MAX_MATERIALS;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_TEXTURE]));

	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_LIGHTS]));
	
	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	//bindings[1] = bindings[0];
	//bindings[1].binding = 1;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_SHADOW]));

	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	bindings[0].descriptorCount = 1;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_MATERIAL]));

	VkPushConstantRange pushConstants;
	pushConstants.offset = 0;
	pushConstants.size = sizeof(uint32_t);
	pushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = _descriptorLayouts.data();
	layoutCreateInfo.setLayoutCount = (uint32_t)_descriptorLayouts.size();
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pushConstants;

	VkCheck(vkCreatePipelineLayout(Renderer::device(), &layoutCreateInfo, nullptr, &_pipelineLayout));
}

void DeferredSceneRenderPass::_createRenderPass()
{
	//Color
	VkAttachmentDescription attachDesc = {};
	attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//attachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	//attachDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachDesc.format = VK_FORMAT_R16G16B16A16_UINT;
	attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkAttachmentReference attachRef = {};
	attachRef.attachment = 0;
	attachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//Normal
	VkAttachmentDescription normalAttachDesc = {};
	normalAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//normalAttachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//normalAttachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalAttachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	normalAttachDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	normalAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	normalAttachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	normalAttachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	normalAttachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	normalAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkAttachmentReference normalAttachRef = {};
	normalAttachRef.attachment = 1;
	normalAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference attachRefs[2] = { attachRef, normalAttachRef };
	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 2;
	subpass.pColorAttachments = attachRefs;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//Depth
	VkAttachmentReference depthAttach = {};
	depthAttach.attachment = 2;
	depthAttach.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription depthPass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthAttach;

	VkAttachmentDescription depthDesc = {};
	depthDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	depthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	depthDesc.format = VK_FORMAT_D32_SFLOAT;


	//VkSubpassDependency dependency = {};
	//dependency.srcAccessMask = 0;
	//dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//dependency.srcSubpass = VK_SUBPASS_EXTERNAL;

	//dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	//dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//dependency.dstSubpass = 0;

	VkSubpassDependency dependencies[2] = {};
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstAccessMask = 
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstSubpass = 0;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcAccessMask = 
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


	VkAttachmentDescription attachments[] = { 
		attachDesc, normalAttachDesc, depthDesc
	};
	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 3;
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 2;
	info.pDependencies = dependencies;

	VkCheck(vkCreateRenderPass(Renderer::device(), &info, nullptr, &_geometryPass));

	depthAttach.attachment = 1;
	attachDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	depthDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentDescription shadingAttachments[] = { 
		attachDesc, depthDesc
	};
	info.pAttachments = shadingAttachments;
	info.attachmentCount = 2;
	//info.dependencyCount = 2;
	subpass.colorAttachmentCount = 1;
	VkCheck(vkCreateRenderPass(Renderer::device(), &info, nullptr, &_renderPass));
}

void DeferredSceneRenderPass::_cleanupDeferredTargets()
{
	VkDevice d = Renderer::device();

	for (DeferredFramebuffer& fb : _deferredFramebuffers)
	{
		vkDestroyFramebuffer(d, fb.framebuffer, nullptr);

		vkDestroyImageView(d, fb.view, nullptr);
		vkDestroyImageView(d, fb.normalView, nullptr);
		vkDestroyImageView(d, fb.depthView, nullptr);

		vkDestroyImage(d, fb.image, nullptr);
		vkDestroyImage(d, fb.normalImage, nullptr);
		vkDestroyImage(d, fb.depthImage, nullptr);

		vkFreeMemory(d, fb.memory, nullptr);
		vkFreeMemory(d, fb.normalMemory, nullptr);
		vkFreeMemory(d, fb.depthMemory, nullptr);
	}

	_deferredFramebuffers.clear();
}

void DeferredSceneRenderPass::_createRenderTargets(Renderer* renderer)
{
	const std::vector<Framebuffer>& swapChainBuffers = renderer->swapChain()->framebuffers();

	VkExtent2D extent = renderer->extent();

	//for (size_t i = 0; i < swapChainBuffers.size(); ++i)
	{
		DeferredFramebuffer fb = {};

		//Image - color
		{
			//fb.image = swapChainBuffers[i].image;

			VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.extent = { extent.width, extent.height, 1 };
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			//info.format = VK_FORMAT_B8G8R8A8_UNORM;
			info.format = VK_FORMAT_R16G16B16A16_UINT;
			info.imageType = VK_IMAGE_TYPE_2D;

			VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &fb.image));

			VkMemoryRequirements memReq;
			vkGetImageMemoryRequirements(Renderer::device(), fb.image, &memReq);

			VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			alloc.allocationSize = memReq.size;
			alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &fb.memory));
			VkCheck(vkBindImageMemory(Renderer::device(), fb.image, fb.memory, 0));
		}

		//View - color
		{
			//fb.view = swapChainBuffers[i].view;

			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = fb.image;
			//info.format = VK_FORMAT_B8G8R8A8_UNORM;
			info.format = VK_FORMAT_R16G16B16A16_UINT;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;

			info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = 0;
			info.subresourceRange.layerCount = 1;

			VkCheck(vkCreateImageView(Renderer::device(), &info, nullptr, &(fb.view)));
		}

		//Image - normals
		{
			//fb.image = swapChainBuffers[i].image;

			VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.extent = { extent.width, extent.height, 1 };
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.format = VK_FORMAT_B8G8R8A8_UNORM;
			info.imageType = VK_IMAGE_TYPE_2D;

			VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &fb.normalImage));

			VkMemoryRequirements memReq;
			vkGetImageMemoryRequirements(Renderer::device(), fb.normalImage, &memReq);

			VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			alloc.allocationSize = memReq.size;
			alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &fb.normalMemory));
			VkCheck(vkBindImageMemory(Renderer::device(), fb.normalImage, fb.normalMemory, 0));
		}

		//View - normals
		{
			//fb.view = swapChainBuffers[i].view;

			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = fb.normalImage;
			info.format = VK_FORMAT_B8G8R8A8_UNORM;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;

			info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = 0;
			info.subresourceRange.layerCount = 1;

			VkCheck(vkCreateImageView(Renderer::device(), &info, nullptr, &(fb.normalView)));
		}

		//Depth image
		{
			VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.extent = { extent.width, extent.height, 1 };
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.format = VK_FORMAT_D32_SFLOAT;
			info.imageType = VK_IMAGE_TYPE_2D;

			VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &fb.depthImage));

			VkMemoryRequirements memReq;
			vkGetImageMemoryRequirements(Renderer::device(), fb.depthImage, &memReq);

			VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			alloc.allocationSize = memReq.size;
			alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &fb.depthMemory));
			VkCheck(vkBindImageMemory(Renderer::device(), fb.depthImage, fb.depthMemory, 0));
		}

		//Depth view
		{
			VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			view.format = VK_FORMAT_D32_SFLOAT;
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.image = fb.depthImage;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			view.subresourceRange.baseMipLevel = 0;
			view.subresourceRange.baseArrayLayer = 0;
			view.subresourceRange.layerCount = 1;
			view.subresourceRange.levelCount = 1;

			VkCheck(vkCreateImageView(Renderer::device(), &view, nullptr, &fb.depthView));
		}

		//FB
		{
			//fb.framebuffer = swapChainBuffers[i].framebuffer;

			const VkImageView attachments[] = {
				fb.view, fb.normalView, fb.depthView
			};

			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.width = extent.width;
			info.height = extent.height;
			info.renderPass = _geometryPass;
			info.attachmentCount = 3;
			info.pAttachments = attachments;
			info.layers = 1;

			VkCheck(vkCreateFramebuffer(Renderer::device(), &info, nullptr, &(fb.framebuffer)));
		}

		//Create the descriptor set
		{
			VkDescriptorSetAllocateInfo alloc = {};
			alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			alloc.descriptorPool = _descriptorPool;
			alloc.pSetLayouts = &_deferredSetLayouts[0];
			alloc.descriptorSetCount = 1;

			VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, &_bindingSet));
		
			VkDescriptorImageInfo img = {};
			img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			img.sampler = _sampler;
			img.imageView = fb.view;

			VkWriteDescriptorSet writes[4] = {};
			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[0].dstSet = _bindingSet;
			writes[0].dstBinding = 0;
			writes[0].dstArrayElement = 0;
			writes[0].pImageInfo = &img;

			VkDescriptorImageInfo norm = img;
			norm.imageView = fb.normalView;
			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[1].dstSet = _bindingSet;
			writes[1].dstBinding = 1;
			writes[1].dstArrayElement = 0;
			writes[1].pImageInfo = &norm;

			VkDescriptorImageInfo depth = img;
			depth.imageView = fb.depthView;
			writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[2].dstSet = _bindingSet;
			writes[2].dstBinding = 2;
			writes[2].dstArrayElement = 0;
			writes[2].pImageInfo = &depth;

			VkDescriptorImageInfo ssao = img;
			ssao.imageView = _ssaoPass->ssaoView();
			writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[3].descriptorCount = 1;
			writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[3].dstSet = _bindingSet;
			writes[3].dstBinding = 3;
			writes[3].dstArrayElement = 0;
			writes[3].pImageInfo = &ssao;

			vkUpdateDescriptorSets(Renderer::device(), 4, writes, 0, nullptr);
		}

		_deferredFramebuffers.push_back(fb);
	}
}

void DeferredSceneRenderPass::_createDeferredLayout()
{
	_deferredSetLayouts.resize(7);

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

	//Set 0 - render targets
	info.bindingCount = 4;
	VkDescriptorSetLayoutBinding bindings[4] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].descriptorCount = 1;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].descriptorCount = 1;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[2].descriptorCount = 1;

	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[3].descriptorCount = 1;

	info.pBindings = bindings;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, 
		nullptr, &_deferredSetLayouts[0]));

	//Set 1 - sampler
	info.bindingCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, 
		nullptr, &_deferredSetLayouts[1]));

	//Set 2 - lights
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, 
		nullptr, &_deferredSetLayouts[2]));
	
	//Set 3 - textures
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].descriptorCount = MAX_MATERIALS;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info,
		nullptr, &_deferredSetLayouts[3]));

	//Set 4 - camera
	bindings[0].descriptorCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, 
		nullptr, &_deferredSetLayouts[4]));

	//Set 5 - shadow map
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, 
		nullptr, &_deferredSetLayouts[5]));

	//Set 6 - material data
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info,
		nullptr, &_deferredSetLayouts[6]));


	VkPushConstantRange pushConstants;
	pushConstants.offset = 0;
	pushConstants.size = sizeof(uint32_t);
	pushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = _deferredSetLayouts.data();
	layoutCreateInfo.setLayoutCount = (uint32_t)_deferredSetLayouts.size();
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pushConstants;

	VkCheck(vkCreatePipelineLayout(Renderer::device(), &layoutCreateInfo, 
		nullptr, &_deferredPipelineLayout));
}

void DeferredSceneRenderPass::_createDeferredPipeline()
{
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[0].module = ShaderCache::getModule("shaders/screen/screenquad.vert");

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";
	stages[1].module = ShaderCache::getModule("shaders/screen/deferred_pass.frag");

	VkPipelineColorBlendAttachmentState cba = {};
	cba.blendEnable = VK_TRUE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	cba.colorBlendOp = VK_BLEND_OP_ADD;
	cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	VkPipelineColorBlendStateCreateInfo cbs = {};
	cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbs.attachmentCount = 1;
	cbs.pAttachments = &cba;
	cbs.logicOp = VK_LOGIC_OP_COPY;

	VkPipelineInputAssemblyStateCreateInfo ias = {};
	ias.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	ias.primitiveRestartEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo mss = {};
	mss.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	mss.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.lineWidth = 1.0f;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.depthBiasEnable = VK_FALSE;

	VkVertexInputBindingDescription vbs = {};
	vbs.binding = 0;
	vbs.stride = sizeof(Vertex);
	vbs.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vis = {};
	vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	//Set dynamically.
	VkRect2D sc = {};
	VkViewport vp = {};

	VkPipelineViewportStateCreateInfo vps = {};
	vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vps.viewportCount = 1;
	vps.scissorCount = 1;
	vps.pViewports = &vp;
	vps.pScissors = &sc;

	VkPipelineDepthStencilStateCreateInfo dss = {};
	dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dss.depthTestEnable = VK_FALSE;
	dss.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkDynamicState dynStates[] = { 
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dys = {};
	dys.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dys.dynamicStateCount = 2;
	dys.pDynamicStates = dynStates;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.layout = _deferredPipelineLayout;
	info.renderPass = _renderPass;
	info.stageCount = 2;
	info.subpass = 0;
	info.pStages = stages;
	info.pColorBlendState = &cbs;
	info.pInputAssemblyState = &ias;
	info.pMultisampleState = &mss;
	info.pRasterizationState = &rs;
	info.pVertexInputState = &vis;
	info.pViewportState = &vps;
	info.pDepthStencilState = &dss;
	info.pDynamicState = &dys;

	VkCheck(vkCreateGraphicsPipelines(Renderer::device(), VK_NULL_HANDLE,
		1, &info, nullptr, &_deferredPipeline));
}
