#define RGFW_VULKAN
#define RGFW_IMPLEMENTATION
#include "RGFW.h"

#define TVK_INSTANCE_EXTENSIONS          \
		VK_KHR_SURFACE_EXTENSION_NAME,   \
		RGFW_VK_SURFACE,
#define TVK_DEVICE_EXTENSIONS            \
		"VK_KHR_shader_draw_parameters", \
		"VK_KHR_swapchain",
#define TINY_VK_IMPLEMENTATION
#include "tinyvk.h"

#define data             shader_update_data
#define data_sizeInBytes shader_update_size
#include "update.slang.h"
#undef data
#undef data_sizeInBytes

#define data             shader_draw_data
#define data_sizeInBytes shader_draw_size
#include "draw.slang.h"
#undef data
#undef data_sizeInBytes

#define ROOT_3 1.73205080757f

typedef struct { float x, y;    } float2;
typedef struct { float x, y, z; } float3;

typedef struct {
	float2 position;
} vertex_t;

typedef struct {
	float2 offset;
	float radius;
	uint32_t color;
} instance_t;

VkSurfaceKHR surface;
VkPipelineLayout pipeline_layout;
VkPipelineLayout compute_pipeline_layout;
VkRenderPass render_pass;
VkFramebuffer frame_buffers[TVK_MAX_SWAP_CHAIN_IMAGES];
VkPipeline compute_pipeline;

VkResult create_graphics_pipeline(VkDevice device, VkPipeline *pipeline);
VkResult create_render_pass(VkDevice device, VkFormat format);

float random() { return (float)(rand()) / (float)(RAND_MAX); }
uint32_t random_uint() {
	uint32_t a1 = rand();
	uint32_t a2 = rand();
	return 0xB0000000 | (a1 << 15) | (a2);
}

int main()
{
	srand(2026u * 17532311u); // 17532311 is prime, not like it matters, this rng sucks

	TvkContext context = {};
	assert(tvkCreateContext(TVK_CREATE_CONTEXT_ENABLE_VALIDATION, &context) == VK_SUCCESS);
	
	RGFW_window *window = 0;
	assert(window = RGFW_createWindow("Circles!", 0, 0, 1024, 1024, RGFW_windowCenter));
	RGFW_window_createSurface_Vulkan(window, context.instance, &surface);

	TvkSwapChain swap_chain = {};
	tvkCreateSwapChain(&context, surface, &swap_chain);

	assert(create_render_pass(context.device, swap_chain.format) == VK_SUCCESS);
	
	VkImageView attachments[] = { 0 };
	VkFramebufferCreateInfo frame_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = render_pass,
		.attachmentCount = TVK_ARRAY_COUNT(attachments),
		.pAttachments = attachments,
		.width = swap_chain.extent.width,
		.height = swap_chain.extent.height,
		.layers = 1,
	};
	for (uint32_t i = 0; i < swap_chain.image_count; ++i)
	{
		attachments[0] = swap_chain.image_views[i];
		assert(vkCreateFramebuffer(context.device, &frame_buffer_info, 0, &frame_buffers[i]) == VK_SUCCESS);
	}
	
    TvkDescriptorSetBuilder builder = {};
    tvkSetBuilderAppend(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    tvkSetBuilderAppend(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    tvkSetBuilderCreate(context.device, VK_SHADER_STAGE_COMPUTE_BIT, 1, &builder);

	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	};
	vkCreatePipelineLayout(context.device, &layout_info, 0, &pipeline_layout);
	layout_info.setLayoutCount = 1,
	layout_info.pSetLayouts = &builder.layout,
	vkCreatePipelineLayout(context.device, &layout_info, 0, &compute_pipeline_layout);
	
    VkShaderModule shader = 0;
    tvkCreateShaderFromMemory(context.device, (void*)(shader_update_data), shader_update_size, &shader);
    tvkCreateComputePipeline(context.device, compute_pipeline_layout, shader, &compute_pipeline);
    vkDestroyShaderModule(context.device, shader, 0);

	VkPipeline pipeline;
	assert(create_graphics_pipeline(context.device, &pipeline) == VK_SUCCESS);

	VkSemaphore image_acquired_semaphore = 0;
	VkSemaphore render_finished_semaphore = 0;
	VkSemaphoreCreateInfo semaphore_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	vkCreateSemaphore(context.device, &semaphore_info, 0, &image_acquired_semaphore);
	vkCreateSemaphore(context.device, &semaphore_info, 0, &render_finished_semaphore);

	// Vertex Buffer
	VkBuffer buffers[2] = {};
	{
		VkDeviceMemory memory = 0;
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = 3 * sizeof(vertex_t),
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		};
		vertex_t initial_data[] = {
			{.position = {   0.0f, 2.0f}},
			{.position = { ROOT_3,-1.0f}},
			{.position = {-ROOT_3,-1.0f}},
		};
		tvkCreateBuffer(&context, &buffer_info, initial_data, TVK_MEMORY_USAGE_DEVICE, &buffers[0], &memory);
	}

	uint32_t circle_count = 1000;
	
	// Instance Buffer
	{
		VkDeviceMemory memory = 0;
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = circle_count * sizeof(instance_t),
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		};
		instance_t *initial_data = (instance_t*)malloc(circle_count * sizeof(instance_t));
		for (uint32_t i = 0; i < circle_count; ++i)
		{
			initial_data[i] = (instance_t){.offset = {random(), random()}, .radius = random() * 0.18f + 0.02f, .color = random_uint()};
		};
		tvkCreateBuffer(&context, &buffer_info, initial_data, TVK_MEMORY_USAGE_DEVICE, &buffers[1], &memory);
	}
	
	VkBuffer velocity_buffer = 0;
	{
		VkDeviceMemory memory = 0;
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = circle_count * sizeof(float2),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		};
		float2 *initial_data = (float2*)malloc(circle_count * sizeof(float2));
		for (uint32_t i = 0; i < circle_count; ++i)
		{
			float theta = random() * (6.28318530718f);
			initial_data[i] = (float2){cosf(theta), sinf(theta)};
		}
		tvkCreateBuffer(&context, &buffer_info, initial_data, TVK_MEMORY_USAGE_DEVICE, &velocity_buffer, &memory);
	}

    VkDescriptorSet set = 0;
    tvkSetBuilderAllocate(context.device, &builder, &set);
    VkWriteDescriptorSet writes[] = {
        tvkSetBuilderWrite(&builder, set, 0, buffers[1]),
        tvkSetBuilderWrite(&builder, set, 1, velocity_buffer),
    };
    vkUpdateDescriptorSets(context.device, TVK_ARRAY_COUNT(writes), writes, 0, 0);

	while (!RGFW_window_shouldClose(window)) {
		RGFW_pollEvents();

		vkQueueWaitIdle(context.queue);

		uint32_t image_index = 0;
		VkResult result = VK_ERROR_UNKNOWN;
		while (result != VK_SUCCESS) {
			result = vkAcquireNextImageKHR(context.device, swap_chain.swap_chain,
				UINT64_MAX, image_acquired_semaphore, 0, &image_index);

			if (result == VK_SUBOPTIMAL_KHR) {
				break; // eh, thats fine.
			}
			else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			//	resize(); // ok, we actually need to do something here..
				continue;
			}
			else if (result != VK_SUCCESS) {
				printf("Failed to get swap chain image! (%i)\n", result);
				return 1;
			}
		}

		VkCommandBuffer cmd = 0;
		tvkAllocateCommandBuffer(context.device, context.command_pool, &cmd);
		tvkBeginCommandBuffer(cmd, 0);
		
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_layout, 0, 1, &set, 0, 0);
		vkCmdDispatch(cmd, tvkCeilDiv(circle_count, 64), 1, 1);

		VkBufferMemoryBarrier barrier = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    		.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    		.buffer = buffers[1],
    		.offset = 0,
    		.size = VK_WHOLE_SIZE,
		};
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, 0, 1, &barrier, 0, 0);

		VkClearValue clear_value = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};
		VkRenderPassBeginInfo begin_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_pass,
			.framebuffer = frame_buffers[image_index],
			.renderArea = { {0, 0}, swap_chain.extent },
			.clearValueCount = 1,
			.pClearValues = &clear_value,
		};
		vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = {
			.width = (float)(swap_chain.extent.width),
			.height = (float)(swap_chain.extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = {
			.extent = swap_chain.extent,
		};
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		VkDeviceSize offsets[] = { 0u, 0u };
		vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);
		// vkCmdBindIndexBuffer(cmd, buffers[INDEX_BUFFER], 0, VK_INDEX_TYPE_UINT32);
		vkCmdDraw(cmd, 3, circle_count, 0, 0);
		vkCmdEndRenderPass(cmd);
		vkEndCommandBuffer(cmd);
		tvkQueueSumbitSingle(context.queue, cmd, image_acquired_semaphore, render_finished_semaphore);
		
		VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &render_finished_semaphore,
			.swapchainCount = 1,
			.pSwapchains = &swap_chain.swap_chain,
			.pImageIndices = &image_index,
		};
		assert(vkQueuePresentKHR(context.queue, &present_info) == VK_SUCCESS);
	}
}

VkResult create_graphics_pipeline(VkDevice device, VkPipeline *pipeline) {
	VkResult result = VK_SUCCESS;

	VkShaderModule shader = 0;
	VkShaderModuleCreateInfo shader_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = shader_draw_size,
		.pCode = (uint32_t*)(shader_draw_data),
	};
	TVK_TRY(vkCreateShaderModule(device, &shader_info, 0, &shader));

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shader,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shader,
			.pName = "main",
		},
	};
	VkVertexInputBindingDescription vertex_bindings[] = {
		{
			.binding   = 0,
			.stride    = sizeof(vertex_t),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		},
		{
			.binding   = 1,
			.stride    = sizeof(instance_t),
			.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
		},
	};
	VkVertexInputAttributeDescription vertex_attributes[] = {
		// Position
		{
			.location = 0,
			.binding  = 0,
			.format   = VK_FORMAT_R32G32_SFLOAT,
			.offset   = 0,
		},
		// Instance Position & Radius
		{
			.location = 1,
			.binding  = 1,
			.format   = VK_FORMAT_R32G32B32_SFLOAT,
			.offset   = 0,
		},
		// Instance Color
		{
			.location = 2,
			.binding  = 1,
			.format   = VK_FORMAT_R8G8B8A8_UNORM,
			.offset   = sizeof(float3),
		},
	};
	VkPipelineVertexInputStateCreateInfo vertex_input_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = TVK_ARRAY_COUNT(vertex_bindings),
		.pVertexBindingDescriptions = vertex_bindings,
		.vertexAttributeDescriptionCount = TVK_ARRAY_COUNT(vertex_attributes),
		.pVertexAttributeDescriptions = vertex_attributes,
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};
	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	VkPipelineRasterizationStateCreateInfo rasterization_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};
	VkPipelineMultisampleStateCreateInfo multisample_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};
	VkPipelineColorBlendAttachmentState blend_attachment = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		.blendEnable = VK_FALSE,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
	};
	VkPipelineColorBlendStateCreateInfo color_blend_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment,
	};
	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = TVK_ARRAY_COUNT(dynamic_states),
		.pDynamicStates = dynamic_states,
	};
	VkGraphicsPipelineCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount          = TVK_ARRAY_COUNT(shader_stages),
		.pStages             = shader_stages,
		.pVertexInputState   = &vertex_input_state,
		.pInputAssemblyState = &input_assembly_state,
		.pViewportState      = &viewport_state,
		.pRasterizationState = &rasterization_state,
		.pMultisampleState   = &multisample_state,
		.pDepthStencilState  = &depth_stencil_state,
		.pColorBlendState    = &color_blend_state,
		.pDynamicState       = &dynamic_state,
		.layout              = pipeline_layout,
		.renderPass          = render_pass,
		.subpass             = 0,
	};
	TVK_TRY(vkCreateGraphicsPipelines(device, 0, 1, &info, 0, pipeline));
	return VK_SUCCESS;
}

VkResult create_render_pass(VkDevice device, VkFormat format)
{
	VkResult result = VK_SUCCESS;
	VkAttachmentDescription attachments[] = {
		// Color Attachment
		{
			.format = format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		},
	};
	VkAttachmentReference color_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
	};
	VkSubpassDependency subpass_dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	};
	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = TVK_ARRAY_COUNT(attachments),
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &subpass_dependency,
	};
	TVK_TRY(vkCreateRenderPass(device, &render_pass_info, 0, &render_pass));
	return VK_SUCCESS;
}
