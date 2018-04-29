#include "SSAORenderPass.h"

#include <random>

#include "../Renderer.h"
#include "../Scene.h"
#include "../texture/Texture.h"
#include "../ShaderCache.h"
#include "../Model.h"

const VkFormat SSAO_FORMAT = VK_FORMAT_R8_UNORM;

inline float lerp(float a, float b, float c)
{
	return (a + c * (b - a));
}

SSAORenderPass::~SSAORenderPass()
{
	_cleanup();
	delete _noiseTexture;

	VkDevice d = Renderer::device();
	vkDestroyRenderPass(d, _ssaoPass, nullptr);
	vkDestroyRenderPass(d, _blurPass, nullptr);

	vkDestroyPipeline(d, _ssaoPipeline, nullptr);
	vkDestroyPipeline(d, _blurPipeline, nullptr);

	vkDestroySampler(d, _sampler, nullptr);
}

void SSAORenderPass::init(Renderer* renderer)
{
	_renderer = renderer;

	_ssaoFramebuffer.framebuffer = VK_NULL_HANDLE;
	_blurFramebuffer.framebuffer = VK_NULL_HANDLE;

	_createRenderPass();
	_createPipelineLayout();
	_createDescriptorSets(renderer);

	_createSSAOPipeline();
	
	_createNoiseTexture();
	_generateKernelSamples();

	VkDescriptorImageInfo img = {};
	img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img.sampler = _sampler;
	img.imageView = _noiseTexture->view();

	VkWriteDescriptorSet writeSet[2] = {};
	writeSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet[0].descriptorCount = 1;
	writeSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet[0].dstSet = _kernelNoiseSet;
	writeSet[0].dstBinding = 0;
	writeSet[0].dstArrayElement = 0;
	writeSet[0].pImageInfo = &img;

	VkDescriptorBufferInfo buf = {};
	buf.range = VK_WHOLE_SIZE;
	buf.buffer = renderer->getUniform("ssaoKernel")->localBuffer.buffer;

	writeSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet[1].descriptorCount = 1;
	writeSet[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSet[1].dstSet = _kernelNoiseSet;
	writeSet[1].dstBinding = 1;
	writeSet[1].dstArrayElement = 0;
	writeSet[1].pBufferInfo = &buf;

	vkUpdateDescriptorSets(Renderer::device(), 2, writeSet, 0, nullptr);
}

void SSAORenderPass::reload()
{
}

void SSAORenderPass::render(VkCommandBuffer cmd, const Framebuffer* framebuffer)
{
	VkClearValue clear = { 0.0f, 0.0f, 0.0f, 1.0f };
	VkExtent2D extent = _renderer->extent();

	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.clearValueCount = 1;
	info.pClearValues = &clear;
	info.renderPass = _ssaoPass;
	info.renderArea.extent = extent;
	info.framebuffer = _ssaoFramebuffer.framebuffer;

	VkViewport viewport = {
		0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f
	};

	VkRect2D scissor = { 0, 0, extent.width, extent.height };

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	VkDescriptorSet setBindings[] = {
		_attachmentBinding, _kernelNoiseSet, _blurInputSet
	};

	//Pass 1 - generate SSAO
	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _ssaoPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
		_pipelineLayout, 1, 2, setBindings, 0, nullptr);

	bindDescriptorSetById(cmd, SET_BINDING_CAMERA);

	vkCmdDraw(cmd, 4, 1, 0, 0);

	vkCmdEndRenderPass(cmd);

	//Convert image layouts from attachment to shader_read_only
	VkImageMemoryBarrier memBarrier = {};
	memBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memBarrier.image = _ssaoFramebuffer.image;
	memBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	memBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	memBarrier.srcAccessMask = 0;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	memBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memBarrier.subresourceRange.layerCount = 1;
	memBarrier.subresourceRange.levelCount = 1;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
		0, nullptr, 0, nullptr, 1, &memBarrier);

	//Pass 2 - blur
	
	info.renderPass = _blurPass;
	info.framebuffer = _blurFramebuffer.framebuffer;
	vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
	
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _blurPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
		_pipelineLayout, 1, 3, setBindings, 0, nullptr);

	bindDescriptorSetById(cmd, SET_BINDING_CAMERA);

	vkCmdDraw(cmd, 4, 1, 0, 0);

	vkCmdEndRenderPass(cmd);
	
	//Convert image layout from attachment to shader_read_only
	memBarrier.image = _blurFramebuffer.image;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
		0, nullptr, 0, nullptr, 1, &memBarrier);
}

void SSAORenderPass::resize(uint32_t width, uint32_t height)
{
	_cleanup();
	_createRenderTargets();

	VkDescriptorImageInfo img = {};
	img.imageView = _ssaoFramebuffer.view;
	img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img.sampler = _sampler;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.dstSet = _blurInputSet;
	write.pImageInfo = &img;
	vkUpdateDescriptorSets(Renderer::device(), 1, &write, 0, nullptr);
}

void SSAORenderPass::_createDescriptorSets(Renderer* renderer)
{
	VkDescriptorPoolSize sizes[2] = {};
	//Camera matrix & lights
	sizes[0].descriptorCount = 2;
	sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	//Combined samplers
	sizes[1].descriptorCount = 8;
	sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool.poolSizeCount = 2;
	pool.pPoolSizes = sizes;
	pool.maxSets = 8;

	VkCheck(vkCreateDescriptorPool(Renderer::device(), &pool, nullptr, &_descriptorPool));

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;

	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	samplerInfo.anisotropyEnable = VK_FALSE;

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

	VkDescriptorSetAllocateInfo alloc = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc.descriptorSetCount = 1;
	alloc.descriptorPool = _descriptorPool;
	alloc.pSetLayouts = &_descriptorLayouts[2];
	VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, &_kernelNoiseSet));

	alloc.pSetLayouts = &_descriptorLayouts[3];
	VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, &_blurInputSet));

	_descriptorSets.resize(1);

	{
		VkDescriptorSetAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = _descriptorPool;
		alloc.pSetLayouts = &_descriptorLayouts[0];
		alloc.descriptorSetCount = 1;

		VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, &_descriptorSets[0]));

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
}

void SSAORenderPass::_createPipeline(const std::string& shaderName)
{
}

void SSAORenderPass::_createPipelineLayout()
{
	for (VkDescriptorSetLayout& layout : _descriptorLayouts)
		vkDestroyDescriptorSetLayout(Renderer::device(), layout, nullptr);

	_descriptorLayouts.resize(4);

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 4;

	//Set 1 - Gbuffer input
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
		nullptr, &_descriptorLayouts[1]));

	//Set 2 - noise texture (binding 0) and kernel samples (binding 1)
	info.bindingCount = 2;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info,
		nullptr, &_descriptorLayouts[2]));

	//Set 3 - unblurred SSAO texture for blur pass
	info.bindingCount = 1;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info,
		nullptr, &_descriptorLayouts[3]));


	//Set 0 - camera
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info,
		nullptr, &_descriptorLayouts[0]));

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = _descriptorLayouts.data();
	layoutCreateInfo.setLayoutCount = (uint32_t)_descriptorLayouts.size();

	VkCheck(vkCreatePipelineLayout(Renderer::device(), &layoutCreateInfo, 
		nullptr, &_pipelineLayout));
}

void SSAORenderPass::_createRenderPass()
{
	//Color
	VkAttachmentDescription attachDesc = {};
	attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachDesc.format = SSAO_FORMAT;
	attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkAttachmentReference attachRef = {};
	attachRef.attachment = 0;
	attachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachRef;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

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


	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachDesc;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 2;
	info.pDependencies = dependencies;

	VkCheck(vkCreateRenderPass(Renderer::device(), &info, nullptr, &_ssaoPass));
	VkCheck(vkCreateRenderPass(Renderer::device(), &info, nullptr, &_blurPass));
}

void SSAORenderPass::_cleanup()
{
	VkDevice d = Renderer::device();
	if (_ssaoFramebuffer.framebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(d, _ssaoFramebuffer.framebuffer, nullptr);

		vkDestroyImageView(d, _ssaoFramebuffer.view, nullptr);
		vkDestroyImage(d, _ssaoFramebuffer.image, nullptr);
		vkFreeMemory(d, _ssaoFramebuffer.memory, nullptr);
	}

	if (_blurFramebuffer.framebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(d, _blurFramebuffer.framebuffer, nullptr);

		vkDestroyImageView(d, _blurFramebuffer.view, nullptr);
		vkDestroyImage(d, _blurFramebuffer.image, nullptr);
		vkFreeMemory(d, _blurFramebuffer.memory, nullptr);
	}

}

void SSAORenderPass::_createNoiseTexture()
{
	std::uniform_real_distribution<float> rnd(-1.0f, 1.0f);
	std::default_random_engine gen;
	
	//4 * 4 RGB noise texture
	const short noiseSize = 16 * 4;
	float noiseData[noiseSize] = {};

	for (int i = 0; i < noiseSize; i += 4)
	{
		glm::vec3 n = glm::normalize(glm::vec3(rnd(gen), rnd(gen), 0.0f));

		noiseData[i + 0] = n.x;
		noiseData[i + 1] = n.y;
		noiseData[i + 2] = n.z;
		noiseData[i + 3] = 0.0f;
	}

	_noiseTexture = new Texture(4, 4, VK_FORMAT_R32G32B32A32_SFLOAT, 
		VK_IMAGE_VIEW_TYPE_2D, _renderer);
	const size_t size = noiseSize * sizeof(float);
	_noiseTexture->setImageData(_renderer, (void*)&noiseData, size);
}

void SSAORenderPass::_createSSAOPipeline()
{
	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[0].module = ShaderCache::getModule("shaders/screen/screenquad.vert");

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";
	stages[1].module = ShaderCache::getModule("shaders/screen/ssao.frag");

	VkPipelineColorBlendAttachmentState cba = {};
	cba.blendEnable = VK_TRUE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	// VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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
	info.layout = _pipelineLayout;
	info.renderPass = _ssaoPass;
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
		1, &info, nullptr, &_ssaoPipeline));


	info.renderPass = _blurPass;
	stages[1].module = ShaderCache::getModule("shaders/screen/ssao_blur.frag");
	VkCheck(vkCreateGraphicsPipelines(Renderer::device(), VK_NULL_HANDLE,
		1, &info, nullptr, &_blurPipeline));
}

void SSAORenderPass::_generateKernelSamples()
{
	std::uniform_real_distribution<float> rnd(-1.0f, 1.0f);
	std::default_random_engine gen;

	const short KERNEL_COUNT = 32;
	const float KERNEL_COUNT_F = (float)KERNEL_COUNT;

	std::vector<glm::vec4> _kernel;
	_kernel.reserve(KERNEL_COUNT);

	for (short i = 0; i < KERNEL_COUNT; ++i)
	{
		glm::vec3 sample = glm::vec3(
			rnd(gen), rnd(gen),	(rnd(gen) / 2.0f) + 1.0f
		);

		sample = glm::normalize(sample);
		sample *= ((rnd(gen) / 2.0f) + 1.0f);

		const float scale = i / KERNEL_COUNT_F;
		sample *= lerp(0.1f, 1.0f, scale*scale);

		_kernel.emplace_back(sample, 0.0f);
	}

	const size_t vec4size = sizeof(glm::vec4);
	const size_t sz = vec4size * KERNEL_COUNT;
	Uniform* kernelBuffer = _renderer->createUniform("ssaoKernel", sz);
	_renderer->updateUniform("ssaoKernel", _kernel.data(), sz);
}

void SSAORenderPass::_createRenderTargets()
{
	const VkExtent2D extent = _renderer->extent();

	//
	//SSAO pass
	//

	//memory
	{
		VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.extent = { extent.width, extent.height, 1 };
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.format = SSAO_FORMAT;
		info.imageType = VK_IMAGE_TYPE_2D;

		VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, 
			&_ssaoFramebuffer.image));

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(Renderer::device(), _ssaoFramebuffer.image,
			&memReq);

		VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc.allocationSize = memReq.size;
		alloc.memoryTypeIndex = _renderer->getMemoryTypeIndex(memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, 
			&_ssaoFramebuffer.memory));
		VkCheck(vkBindImageMemory(Renderer::device(), _ssaoFramebuffer.image, 
			_ssaoFramebuffer.memory, 0));
	}

	//view
	{
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = _ssaoFramebuffer.image;
		info.format = SSAO_FORMAT;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;

		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;

		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;

		VkCheck(vkCreateImageView(Renderer::device(), &info, nullptr, 
			&(_ssaoFramebuffer.view)));
	}

	//framebuffer
	{
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.width = extent.width;
		info.height = extent.height;
		info.renderPass = _ssaoPass;
		info.attachmentCount = 1;
		info.pAttachments = &_ssaoFramebuffer.view;
		info.layers = 1;

		VkCheck(vkCreateFramebuffer(Renderer::device(), &info, nullptr, 
			&(_ssaoFramebuffer.framebuffer)));
	}



	//
	//Blur pass
	//

	
	//memory
	{
		VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.extent = { extent.width, extent.height, 1 };
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.format = SSAO_FORMAT;
		info.imageType = VK_IMAGE_TYPE_2D;

		VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, 
			&_blurFramebuffer.image));

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(Renderer::device(), _blurFramebuffer.image,
			&memReq);

		VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc.allocationSize = memReq.size;
		alloc.memoryTypeIndex = _renderer->getMemoryTypeIndex(memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, 
			&_blurFramebuffer.memory));
		VkCheck(vkBindImageMemory(Renderer::device(), _blurFramebuffer.image, 
			_blurFramebuffer.memory, 0));
	}

	//view
	{
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = _blurFramebuffer.image;
		info.format = SSAO_FORMAT;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;

		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;

		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;

		VkCheck(vkCreateImageView(Renderer::device(), &info, nullptr, 
			&(_blurFramebuffer.view)));
	}

	//framebuffer
	{
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.width = extent.width;
		info.height = extent.height;
		info.renderPass = _blurPass;
		info.attachmentCount = 1;
		info.pAttachments = &_blurFramebuffer.view;
		info.layers = 1;

		VkCheck(vkCreateFramebuffer(Renderer::device(), &info, nullptr, 
			&(_blurFramebuffer.framebuffer)));
	}
}
