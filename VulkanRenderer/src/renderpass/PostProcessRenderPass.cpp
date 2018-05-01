#include "PostProcessRenderPass.h"
#include "../Renderer.h"
#include "../ShaderCache.h"
#include "../Model.h"
#include "../SwapChain.h"

const uint32_t MAX_TEXTURES = 64;
const uint32_t MAX_MATERIALS = 64;

PostProcessRenderPass::PostProcessRenderPass(Scene& scene) : _scene(&scene)
{
}

PostProcessRenderPass::~PostProcessRenderPass()
{
	vkDestroySampler(Renderer::device(), _sampler, nullptr);
	_destroyPostprocessRenderTargets();
}

void PostProcessRenderPass::init(Renderer* renderer)
{
	_createRenderPass();
	_createPipelineLayout();
	_createDescriptorSets(renderer);

	_renderer = renderer;
}

void PostProcessRenderPass::render(VkCommandBuffer cmd, const Framebuffer* framebuffer)
{
	if (_passes.empty())
		return;

	if (_imageViewSets.find(framebuffer->view) == _imageViewSets.end())
	{
		_createDescriptorSets(_renderer);
		_allocatePostprocessRenderTargets(_renderer);
	}

	VkClearValue clearValues[] = {
		{ 0.0f, 0.0f, 0.2f, 1.0f }, //Clear color
		{ 1.0f, 0 } //Depth stencil
	};

	VkFramebuffer fb = framebuffer->framebuffer;

	VkExtent2D _extent = _scene->viewport();

	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = _renderPass;
	info.renderArea.offset = { 0, 0 };
	info.renderArea.extent = _extent;
	info.clearValueCount = 2;
	info.pClearValues = clearValues;
	info.framebuffer = fb;

	VkViewport viewport = {
		0, 0, (float)_extent.width, (float)_extent.height, 0.0f, 1.0f
	};
	VkRect2D scissor = { 0, 0, _extent.width, _extent.height };

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	VkImageView previousView = framebuffer->view;

	VkImageCopy region = {};
	region.extent = { _extent.width, _extent.height, 1 };
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource = region.srcSubresource;

	for (size_t i = 0; i < _passes.size(); ++i)
	{
		const std::string& pass = _passes[i];

		const bool hasAssociation = 
			(_postprocessBackbuffers.find(fb) != _postprocessBackbuffers.end());

		vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
			_pipelineLayout, 0, 1, &_imageViewSets[previousView], 0, nullptr);

		//TODO: retrieve current screen shader (if any) from global config.
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
			getPipelineForShader("shaders/screen/" + pass));

		uint32_t flags = _scene->sceneFlags();
		vkCmdPushConstants(cmd, _pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(uint32_t), &flags);

		vkCmdDraw(cmd, 4, 1, 0, 0);

		vkCmdEndRenderPass(cmd);

		//Don't bother copying if we won't need it after this pass.
		if (hasAssociation && i != (_passes.size() - 1))
		{
			vkCmdCopyImage(cmd, framebuffer->image, 
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
				_postprocessBackbuffers[fb]->image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			previousView = _postprocessBackbuffers[fb]->view;
		}
	}
}

void PostProcessRenderPass::_createDescriptorSets(Renderer* renderer)
{
	if(_descriptorPool)
		vkDestroyDescriptorPool(Renderer::device(), _descriptorPool, nullptr);

	const std::vector<Framebuffer>& fbs = renderer->backbufferRenderTargets();
	const std::vector<Framebuffer>& swapChainFBs = renderer->swapChain()->framebuffers();

	if (!fbs.size() || !swapChainFBs.size()) return; //not ready yet.

	VkDescriptorPoolSize sizes[1] = {};

	uint32_t bindings = 2; //depth + color;
	uint32_t count = (uint32_t)fbs.size() * bindings;
	
	//Sampler
	sizes[0].descriptorCount = count;
	sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo pool = {};
	pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool.poolSizeCount = 1;
	pool.pPoolSizes = sizes;
	pool.maxSets = count * (uint32_t)_passes.size();

	VkCheck(vkCreateDescriptorPool(Renderer::device(), &pool, nullptr, &_descriptorPool));

	std::vector<VkDescriptorSetLayout> layouts;
	for (const Framebuffer& fb : fbs)
	{
		for(uint32_t i = 0; i < bindings; ++i)
			layouts.push_back(_descriptorLayouts[0]);
	}

	VkDescriptorSetAllocateInfo alloc = {};
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = _descriptorPool;
	alloc.pSetLayouts = layouts.data();
	alloc.descriptorSetCount = (uint32_t)fbs.size();

	_descriptorSets.resize(fbs.size());

	VkCheck(vkAllocateDescriptorSets(Renderer::device(), &alloc, _descriptorSets.data()));

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;

	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16.0f;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = samplerInfo.addressModeU;
	samplerInfo.addressModeW = samplerInfo.addressModeU;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;

	if(_sampler == VK_NULL_HANDLE)
		VkCheck(vkCreateSampler(Renderer::device(), &samplerInfo, nullptr, &_sampler));

	for (size_t i = 0; i < fbs.size(); ++i)
	{
		VkDescriptorImageInfo img = {};
		img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img.sampler = _sampler;
		img.imageView = fbs[i].view;

		VkWriteDescriptorSet writes[2] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].dstSet = _descriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].pImageInfo = &img;


		VkDescriptorImageInfo depth = img;
		depth.imageView = fbs[i].depthView;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].dstSet = _descriptorSets[i];
		writes[1].dstBinding = 1;
		writes[1].dstArrayElement = 0;
		writes[1].pImageInfo = &depth;

		vkUpdateDescriptorSets(Renderer::device(), 2, writes, 0, nullptr);

		_imageViewSets[fbs[i].view] = _descriptorSets[i];
		_imageViewSets[swapChainFBs[i].view] = _descriptorSets[i];
	}
}

void PostProcessRenderPass::_createPipeline(const std::string& shaderName)
{
	VkPipeline pipeline;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].pName = "main";
	stages[0].module = ShaderCache::getModule("shaders/screen/screenquad.vert");

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].pName = "main";
	stages[1].module = ShaderCache::getModule(shaderName + ".frag");

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

	VkCheck(vkCreateGraphicsPipelines(Renderer::device(), VK_NULL_HANDLE,
		1, &info, nullptr, &pipeline));

	_pipelines[shaderName] = pipeline;
}

void PostProcessRenderPass::_createPipelineLayout()
{
	for (VkDescriptorSetLayout& layout : _descriptorLayouts)
		vkDestroyDescriptorSetLayout(Renderer::device(), layout, nullptr);

	_descriptorLayouts.resize(1);

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = 2;

	VkDescriptorSetLayoutBinding bindings[2] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].descriptorCount = 1;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].descriptorCount = 1;

	info.pBindings = bindings;
	VkCheck(vkCreateDescriptorSetLayout(Renderer::device(), &info, 
		nullptr, &_descriptorLayouts[0]));

	VkPushConstantRange pc = {};
	pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pc.size = sizeof(uint32_t);

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = _descriptorLayouts.data();
	layoutCreateInfo.setLayoutCount = (uint32_t)_descriptorLayouts.size();
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pc;

	VkCheck(vkCreatePipelineLayout(Renderer::device(), &layoutCreateInfo, 
		nullptr, &_pipelineLayout));
}

void PostProcessRenderPass::_createRenderPass()
{
	//Color
	VkAttachmentDescription attachDesc = {};
	attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkAttachmentReference attachRef = {};
	attachRef.attachment = 0;
	attachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachRef;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	//Depth
	VkAttachmentReference depthAttach = {};
	depthAttach.attachment = 1;
	depthAttach.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription depthPass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthAttach;

	VkAttachmentDescription depthDesc = {};
	depthDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	depthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthDesc.format = VK_FORMAT_D32_SFLOAT;


	VkSubpassDependency dependency = {};
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;

	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstSubpass = 0;

	VkAttachmentDescription attachments[] = { attachDesc, depthDesc };
	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 2;
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;

	VkCheck(vkCreateRenderPass(Renderer::device(), &info, nullptr, &_renderPass));
}

void PostProcessRenderPass::_allocatePostprocessRenderTargets(Renderer* renderer)
{
	const std::vector<Framebuffer>& swapChainBuffers = renderer->swapChain()->framebuffers();

	_destroyPostprocessRenderTargets();

	VkExtent2D extent = renderer->extent();

	for (size_t i = 0; i < swapChainBuffers.size(); ++i)
	{
		Framebuffer fb = {};

		//Image
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

			VkCheck(vkCreateImage(Renderer::device(), &info, nullptr, &fb.image));

			VkMemoryRequirements memReq;
			vkGetImageMemoryRequirements(Renderer::device(), fb.image, &memReq);

			VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			alloc.allocationSize = memReq.size;
			alloc.memoryTypeIndex = renderer->getMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VkCheck(vkAllocateMemory(Renderer::device(), &alloc, nullptr, &fb.memory));
			VkCheck(vkBindImageMemory(Renderer::device(), fb.image, fb.memory, 0));
		}

		//View
		{
			//fb.view = swapChainBuffers[i].view;

			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = fb.image;
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

			VkCheck(vkCreateImageView(Renderer::device(), &info, nullptr, &(fb.view)));
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
				fb.view, fb.depthView
			};

			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.width = extent.width;
			info.height = extent.height;
			info.renderPass = renderPass();
			info.attachmentCount = 2;
			info.pAttachments = attachments;
			info.layers = 1;

			VkCheck(vkCreateFramebuffer(Renderer::device(), &info, nullptr, &(fb.framebuffer)));
		}

		_postprocessRenderTargets[swapChainBuffers[i].framebuffer] = fb;
		_postprocessBackbuffers[swapChainBuffers[i].framebuffer] = &renderer->backbufferRenderTargets()[i];
	}
}

void PostProcessRenderPass::_destroyPostprocessRenderTargets()
{
	for (std::pair<const VkFramebuffer, Framebuffer>& pair : _postprocessRenderTargets)
	{
		Framebuffer& fb = pair.second;

		if (fb.framebuffer)
			vkDestroyFramebuffer(Renderer::device(), fb.framebuffer, nullptr);

		if (fb.view)
			vkDestroyImageView(Renderer::device(), fb.view, nullptr);

		if (fb.image)
			vkDestroyImage(Renderer::device(), fb.image, nullptr);

		if (fb.memory)
			vkFreeMemory(Renderer::device(), fb.memory, nullptr);

		if (fb.depthView)
			vkDestroyImageView(Renderer::device(), fb.depthView, nullptr);

		if (fb.depthImage)
			vkDestroyImage(Renderer::device(), fb.depthImage, nullptr);

		if (fb.depthMemory)
			vkFreeMemory(Renderer::device(), fb.depthMemory, nullptr);
	}

	_postprocessRenderTargets.clear();
}

void PostProcessRenderPass::addEffect(const std::string& shaderName)
{
	_passes.push_back(shaderName);
}
