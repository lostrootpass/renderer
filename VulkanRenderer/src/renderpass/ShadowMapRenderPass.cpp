#include "ShadowMapRenderPass.h"
#include "../Renderer.h"
#include "../Scene.h"
#include "../texture/Texture.h"
#include "../Model.h"
#include "../ShaderCache.h"

const uint32_t SHADOW_DIM = 1024;
const VkFormat SHADOW_MAP_FORMAT = VK_FORMAT_D32_SFLOAT;

//TODO: retrieve from global config.
const uint32_t MAX_MATERIALS = 64;
const uint32_t MAX_TEXTURES = 64;
const uint32_t MAX_MODELS = 64;

ShadowMapRenderPass::~ShadowMapRenderPass()
{
	vkDestroyFramebuffer(Renderer::device(), _framebuffer, nullptr);

	delete _depthTexture;
}

void ShadowMapRenderPass::init(Renderer* renderer)
{
	_createRenderPass();
	_createPipelineLayout();
	_createDescriptorSets(renderer);

	recreateShadowMap(renderer);
};

void ShadowMapRenderPass::recreateShadowMap(Renderer* renderer)
{
	_depthTexture = new Texture(SHADOW_DIM, SHADOW_DIM, SHADOW_MAP_FORMAT, renderer);
	_depthTexture->bind(renderer, 0);

	_createFramebuffer();
}

void ShadowMapRenderPass::render(VkCommandBuffer cmd, const Framebuffer*)
{
	VkClearValue clear = { 1.0f, 0.0f };
	VkRenderPassBeginInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	info.clearValueCount = 1;
	info.pClearValues = &clear;
	info.renderPass = _renderPass;
	info.renderArea.offset = { 0, 0 };
	info.renderArea.extent = { SHADOW_DIM, SHADOW_DIM };
	info.framebuffer = _framebuffer;

	VkViewport viewport = { 0, 0, (float)SHADOW_DIM, (float)SHADOW_DIM, 0.0f, 1.0f };
	VkRect2D scissor = { 0, 0, SHADOW_DIM, SHADOW_DIM };

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

	if (_scene)
		_scene->drawShadow(cmd, *this);

	vkCmdEndRenderPass(cmd);

}

void ShadowMapRenderPass::_createDescriptorSets(Renderer* renderer)
{
	VkDescriptorPoolSize sizes[4] = {};
	//Lights & camera
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

	VkDescriptorPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool.poolSizeCount = 4;
	pool.pPoolSizes = sizes;
	pool.maxSets = SET_BINDING_COUNT + MAX_TEXTURES;

	VkCheck(vkCreateDescriptorPool(Renderer::device(), &pool, nullptr, &_descriptorPool));

	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = _descriptorPool;
	alloc.pSetLayouts = _descriptorLayouts.data();
	alloc.descriptorSetCount = (uint32_t)_descriptorLayouts.size();

	_descriptorSets.resize(SET_BINDING_COUNT);

	VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, _descriptorSets.data()));

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

void ShadowMapRenderPass::_createPipeline(const std::string& shaderName)
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
	cba.blendEnable = VK_FALSE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo cbs = {};
	cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

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

	VkCheck(vkCreateGraphicsPipelines(Renderer::device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

	_pipelines[shaderName] = pipeline;
}

void ShadowMapRenderPass::_createPipelineLayout()
{
	VkDescriptorSetLayoutBinding bindings[2] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
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

	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, nullptr, &_descriptorLayouts[SET_BINDING_SHADOW]));

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

void ShadowMapRenderPass::_createFramebuffer()
{
	VkImageView attachments[] = {
		_depthTexture->view()
	};
	VkFramebufferCreateInfo info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	info.renderPass = _renderPass;
	info.width = SHADOW_DIM;
	info.height = SHADOW_DIM;
	info.layers = 1;
	info.attachmentCount = 1;
	info.pAttachments = attachments;

	VkCheck(vkCreateFramebuffer(Renderer::device(), &info, nullptr, &_framebuffer));
}

void ShadowMapRenderPass::_createRenderPass()
{
	VkAttachmentReference attach = {};
	attach.attachment = 0;
	attach.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &attach;

	VkAttachmentDescription desc = {};
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	desc.format = SHADOW_MAP_FORMAT;

	VkRenderPassCreateInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.attachmentCount = 1;
	info.pAttachments = &desc;

	VkCheck(vkCreateRenderPass(Renderer::device(), &info, nullptr, &_renderPass));
}