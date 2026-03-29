#define RGFW_VULKAN
#define RGFW_IMPLEMENTATION
#include "RGFW.h"

#define OPTIONAL_ARGS \
    OPTIONAL_UINT_ARG(count,        128, "-n", "count", "Number of circles")                   \
    OPTIONAL_UINT_ARG(mode,           0, "-m", "mode",  "Triangulation Mode")                  \
    OPTIONAL_INT_ARG(lod,             4, "-l", "lod",   "Level of detail")                     \
    OPTIONAL_UINT_ARG(frame_count, 1024, "-t", "count", "Number of frames to measure timings") \
    OPTIONAL_UINT_ARG(quit_after,     0, "-q", "N",     "Quit after N measurements")           \
    OPTIONAL_UINT_ARG(dim,          512, "-d", "dim",   "Window dimensions")
#define BOOLEAN_ARGS \
    BOOLEAN_ARG(help,         "-h", "Show help")                                           \
    BOOLEAN_ARG(use_aa,       "-a", "Use anti-aliasing (MSAA 4X or shader defined)")       \
    BOOLEAN_ARG(use_blending, "-b", "Enable alpha blending")                               \
    BOOLEAN_ARG(single,       "-s", "Only draw a single circle filling the entire window") \
    BOOLEAN_ARG(only_results, "-o", "Only print timing results (milliseconds)")            \
    BOOLEAN_ARG(validate,     "-v", "Enable KHRONOS Validation Layer")                     \
    BOOLEAN_ARG(wireframe,    "-w", "Show triangle wireframe")
#include "easyargs.h"

#define TVK_INSTANCE_EXTENSIONS          \
        VK_KHR_SURFACE_EXTENSION_NAME,   \
        RGFW_VK_SURFACE,
#define TVK_DEVICE_EXTENSIONS            \
        "VK_KHR_shader_draw_parameters", \
        "VK_KHR_swapchain",
#define TVK_ENABLED_DEVICE_FEATURES \
        .fillModeNonSolid = VK_TRUE,
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

#include "triangulate_circle.h"

#define ROOT_3 1.73205080757f

typedef struct { float x, y;    } float2;

typedef struct {
    float2   offset;
    float    radius;
    uint32_t color;
} instance_t;

typedef enum {
    Mode_Naive           = 0,
    Mode_Fan             = 1,
    Mode_Strip           = 2,
    Mode_Quad            = 3,
    Mode_Max_Area        = 4,
    Mode_Triangle_Cutout = 5,
    Mode_Quad_Cutout     = 6,
} Mode;

TvkContext context;
RGFW_window *window;
TvkSwapChain swap_chain;
VkSurfaceKHR surface;
VkRenderPass render_pass;
VkFramebuffer frame_buffers[TVK_MAX_SWAP_CHAIN_IMAGES];
VkCommandBuffer command_buffers[TVK_MAX_SWAP_CHAIN_IMAGES];
TvkDescriptorSetBuilder set_builder;
VkPipelineLayout g_pipeline_layout;
VkPipelineLayout c_pipeline_layout;
VkPipeline g_pipeline;
VkPipeline c_pipeline;

VkImage msaa_image;
VkImageView msaa_image_view;

VkBuffer index_buffer;
VkBuffer vertex_buffer;
VkBuffer instance_buffer;
VkBuffer velocity_buffer;
VkDescriptorSet set;
VkQueryPool query_pool;

uint32_t index_count;
uint32_t instance_count;
double *frame_times;

args_t args;

int Setup(int argc, char* argv[]);
void DisplayGPUInfo(VkPhysicalDeviceProperties properties);
void CreateRenderPass();
void CreateFrameBuffers();
void CreateLayouts();
void CreatePipelines();
void CreateBuffers();
void RecordCommandBuffers();

float randomf() { return (float)(rand()) / (float)(RAND_MAX); }
uint32_t random_uint() { return 0xB0000000 | ((rand()) << 15) | (rand()); }

int main(int argc, char* argv[])
{
    srand(2026u * 17532311u);

    if (Setup(argc, argv)) return 0;
    assert(tvkCreateContext(args.validate? TVK_CREATE_CONTEXT_ENABLE_VALIDATION : 0, &context) == VK_SUCCESS);

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(context.physical_device, &properties);

	// GPU must support timestamps
    assert(properties.limits.timestampPeriod != 0.0f);
    assert(properties.limits.timestampComputeAndGraphics == VK_TRUE);
	DisplayGPUInfo(properties);

    assert(window = RGFW_createWindow("Circles!", 0, 0, args.dim, args.dim, RGFW_windowCenter | RGFW_windowNoResize));
    RGFW_window_createSurface_Vulkan(window, context.instance, &surface);
    assert(tvkCreateSwapChain(&context, surface, &swap_chain) == VK_SUCCESS);
    CreateRenderPass();
    CreateFrameBuffers();
    CreateLayouts();
    CreatePipelines();
    CreateBuffers();
    RecordCommandBuffers();

    VkSemaphore image_acquired_semaphore = 0;
    VkSemaphore render_finished_semaphore = 0;
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    vkCreateSemaphore(context.device, &semaphore_info, 0, &image_acquired_semaphore);
    vkCreateSemaphore(context.device, &semaphore_info, 0, &render_finished_semaphore);

    uint32_t frame_index = 0;
    uint32_t measurement_count = 0;
    while (!RGFW_window_shouldClose(window))
    {
        RGFW_pollEvents();

        uint32_t image_index = 0;
        VkResult result = VK_ERROR_UNKNOWN;
        while (result != VK_SUCCESS) {
            result = vkAcquireNextImageKHR(context.device, swap_chain.swap_chain,
                UINT64_MAX, image_acquired_semaphore, 0, &image_index);

            if (result == VK_SUBOPTIMAL_KHR) {
                break; // eh, thats fine.
            }
            else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                return 1; // Resizing disabled
            }
            else if (result != VK_SUCCESS) {
                printf("Failed to get swap chain image! (%i)\n", result);
                return 1;
            }
        }

        assert(tvkQueueSumbitSingle(context.queue, command_buffers[image_index],
            image_acquired_semaphore, render_finished_semaphore) == VK_SUCCESS);
        
        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &swap_chain.swap_chain,
            .pImageIndices = &image_index,
        };
        assert(vkQueuePresentKHR(context.queue, &present_info) == VK_SUCCESS);
        assert(vkQueueWaitIdle(context.queue) == VK_SUCCESS);

        if (frame_index < args.frame_count) {
            uint64_t timestamps[2];
            vkGetQueryPoolResults(context.device, query_pool, 0, 2, sizeof(timestamps),
            &timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            
            frame_times[frame_index] = (double)(timestamps[1] - timestamps[0])
                                     * properties.limits.timestampPeriod / 1e6;
        }
        frame_index += 1;

        if (frame_index == args.frame_count) {
            double sum = 0.0;
            for (uint32_t i = 0; i < args.frame_count; ++i) {
                sum += frame_times[i];
            }
            double mean = sum / (double)(args.frame_count);
			sum = 0.0;
            for (uint32_t i = 0; i < args.frame_count; ++i) {
				double d = frame_times[i] - mean;
                sum += d * d;
            }
			double stddev = sqrt(sum / (double)(args.frame_count));
            if (args.only_results) {
                printf("%.5f\n", mean);
            } else {
                printf("Average render time: \x1b[94m%.4f\x1b[0m ms, stddev: \x1b[92m%.5f\x1b[0m (over %u frames)\n", mean, stddev, args.frame_count);
            }
            measurement_count += 1;
            if (args.quit_after != 0 && measurement_count >= args.quit_after) {
                break;
            }
            frame_index = 0;
        }
    }
}

int Setup(int argc, char* argv[])
{
    args = make_default_args();
    if (!parse_args(argc, argv, &args) || args.help) {
        print_help(argv[0]);
        printf("\nMODES:\n"
               "    0  Naive\n"
               "    1  Fan\n"
               "    2  Strip\n"
               "    3  Quad Fan\n"
               "    4  Max Area\n"
               "    5  Triangle Cutout\n"
               "    6  Quad Cutout\n"
        );
        return 1;
    }
	if (args.dim < 1) {
		printf("Window dimensions must not be less than 1! (%u)\n", args.dim);
		return 1;
	}
    instance_count = args.count;
    frame_times = (double*)malloc(args.frame_count * sizeof(double));
    const char* mode_name = 0;
    switch (args.mode)
    {
        default:
        {
            printf("%i is not a valid mode!\n", args.mode);
            return 1;
        }
        break;case Mode_Naive:           { mode_name = "Naive"; }
        break;case Mode_Fan:             { mode_name = "Fan";   }
        break;case Mode_Strip:           { mode_name = "Strip"; }
        break;case Mode_Quad:            { mode_name = "Quad";  }
        break;case Mode_Max_Area:        { mode_name = "Max Area"; }
        break;case Mode_Triangle_Cutout: { mode_name = "Triangle Cutout"; }
        break;case Mode_Quad_Cutout:     { mode_name = "Quad Cutout"; }
        break;
    }
    if (!args.only_results) {
		printf("Mode: %s\n", mode_name);
        if (args.mode < Mode_Triangle_Cutout) {
            printf("LOD: %i\n", args.lod);
        }
    }
    return 0;
}

void DisplayGPUInfo(VkPhysicalDeviceProperties properties)
{
	if (args.only_results) return;
	// https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/1e6ca6e3c0763daabd6a101b860ab4354a07f5d3/functions.php#L294
	char version[32];
	switch (properties.vendorID) {
		default:
		{
			uint32_t X = VK_API_VERSION_MAJOR(properties.driverVersion);
			uint32_t Y = VK_API_VERSION_MINOR(properties.driverVersion);
			uint32_t Z = VK_API_VERSION_PATCH(properties.driverVersion);
			snprintf(version, sizeof(version), "%u.%u.%u", X, Y, Z);
		}
		break;case 4318: // NVIDIA
		{
			uint32_t X = (properties.driverVersion >> 22) & 0x3ff;
			uint32_t Y = (properties.driverVersion >> 14) & 0x0ff;
			uint32_t Z = (properties.driverVersion >> 6) & 0x0ff;
			uint32_t W = (properties.driverVersion) & 0x003f;
			snprintf(version, sizeof(version), "%u.%u.%u.%u", X, Y, Z, W);
		}
#if defined(_WIN32)
		break;case 0x8086: // INTEL?
		{
			uint32_t X = (properties.driverVersion >> 14);
			uint32_t Y = (properties.driverVersion) & 0x3fff;
			snprintf(version, sizeof(version), "%u.%u", X, Y);
		}
#endif
		break;
	}
	printf("GPU: %s, Driver version %s\n", properties.deviceName, version);
}

void CreateRenderPass()
{
	bool MSAA = args.use_aa && !(args.mode >= Mode_Triangle_Cutout);
    VkAttachmentDescription attachments[2] = {
        // Color Attachment
        {
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_4_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
        // MSAA Resolve Attachment
        {
            .format = swap_chain.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
    };
	if (!MSAA)
	{
        attachments[0].format = swap_chain.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference resolve_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };
	if (MSAA)
	{
		subpass.pResolveAttachments = &resolve_attachment_ref;
	}
    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = (MSAA ? 2 : 1),
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };
    assert(vkCreateRenderPass(context.device, &render_pass_info, 0, &render_pass) == VK_SUCCESS);
}

void CreateFrameBuffers()
{
	bool MSAA = args.use_aa && !(args.mode >= Mode_Triangle_Cutout);
	if (MSAA)
	{
		VkDeviceMemory memory = 0;
		VkImageCreateInfo image_info = {
    		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    		.imageType = VK_IMAGE_TYPE_2D,
    		.format = VK_FORMAT_B8G8R8A8_UNORM,
    		.extent = { swap_chain.extent.width, swap_chain.extent.height, 1 },
    		.mipLevels = 1,
    		.arrayLayers = 1,
    		.samples = VK_SAMPLE_COUNT_4_BIT,
    		.tiling = VK_IMAGE_TILING_OPTIMAL,
    		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		assert(tvkCreateImage(&context, &image_info, TVK_MEMORY_USAGE_DEVICE, &msaa_image, &memory) == VK_SUCCESS);
		assert(tvkCreateImageView(context.device, &image_info, msaa_image, &msaa_image_view) == VK_SUCCESS);
	}
    for (uint32_t i = 0; i < swap_chain.image_count; ++i)
    {
		VkImageView attachments[] = { msaa_image_view, swap_chain.image_views[i] };
        VkFramebufferCreateInfo frame_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = MSAA ? 2u : 1u,
            .pAttachments = attachments + (MSAA ? 0 : 1),
            .width = swap_chain.extent.width,
            .height = swap_chain.extent.height,
            .layers = 1,
        };
        assert(vkCreateFramebuffer(context.device, &frame_buffer_info, 0, &frame_buffers[i]) == VK_SUCCESS);
    }
}

void CreateLayouts()
{
    tvkSetBuilderAppend(&set_builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    tvkSetBuilderAppend(&set_builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    assert(tvkSetBuilderCreate(context.device, VK_SHADER_STAGE_COMPUTE_BIT, 1, &set_builder) == VK_SUCCESS);

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    assert(vkCreatePipelineLayout(context.device, &layout_info, 0, &g_pipeline_layout) == VK_SUCCESS);
    layout_info.setLayoutCount = 1,
    layout_info.pSetLayouts = &set_builder.layout,
    assert(vkCreatePipelineLayout(context.device, &layout_info, 0, &c_pipeline_layout) == VK_SUCCESS);
}

void CreatePipelines()
{
	bool MSAA = args.use_aa && !(args.mode >= Mode_Triangle_Cutout);
	VkBool32 EnableBlending = args.use_blending || (args.use_aa && args.mode >= Mode_Triangle_Cutout);
	if (!args.only_results) {
		printf("MSAA: %s, Blending: %s\n", (MSAA ? "4x":"disabled"), (EnableBlending ? "enabled" : "disabled"));
	}

    VkShaderModule shader = 0;
    assert(tvkCreateShaderFromMemory(context.device, (void*)(shader_update_data), shader_update_size, &shader) == VK_SUCCESS);
    assert(tvkCreateComputePipeline(context.device, c_pipeline_layout, shader, &c_pipeline) == VK_SUCCESS);
    vkDestroyShaderModule(context.device, shader, 0);

    VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = shader_draw_size,
		.pCode = (uint32_t*)(shader_draw_data),
    };
    assert(vkCreateShaderModule(context.device, &shader_info, 0, &shader) == VK_SUCCESS);

	int shader_mode = 0;
	if (args.mode >= Mode_Triangle_Cutout) {
		shader_mode = args.use_aa ? 2 : 1;
	}
	VkSpecializationMapEntry specialization_entry = {
    	.constantID = 0,
    	.offset = 0,
    	.size = sizeof(int),
	};
	VkSpecializationInfo specialization_info = {
    	.mapEntryCount = 1,
    	.pMapEntries = &specialization_entry,
    	.dataSize = sizeof(int),
    	.pData = &shader_mode,
	};
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
			.pSpecializationInfo = &specialization_info,
        },
    };
    VkVertexInputBindingDescription vertex_bindings[] = {
        {
            .binding   = 0,
            .stride    = sizeof(float2),
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
            .offset   = offsetof(instance_t, color),
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
        .polygonMode = args.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisample_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = (MSAA ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT),
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
        .blendEnable = EnableBlending,
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
        .layout              = g_pipeline_layout,
        .renderPass          = render_pass,
        .subpass             = 0,
    };
    assert(vkCreateGraphicsPipelines(context.device, 0, 1, &info, 0, &g_pipeline) == VK_SUCCESS);
}

void CreateBuffers()
{
    tric_Buffers buffers = {};
    tric_Method method = (tric_Method)(args.mode);
    switch (args.mode)
    {
        break;case Mode_Triangle_Cutout:
        {
            buffers.VertexCount = 3;
            buffers.IndexCount = 3;
            buffers.VertexStride = sizeof(float2);
            method = tric_Method_Naive;
        }
        break;case Mode_Quad_Cutout:
        {
            buffers.VertexCount = 4;
            buffers.IndexCount = 6;
            buffers.VertexStride = sizeof(float2);
            method = tric_Method_Naive;
        }
        break;default:
        {
            tric_memory_requirements(method, args.lod, &buffers);
        }
    }
    if (!args.only_results) {
        printf("Vertex Count: %u, Triangle Count: %u\n", buffers.VertexCount, buffers.IndexCount/3u);
    }
    index_count = buffers.IndexCount;
    buffers.Vertices = malloc(buffers.VertexCount * sizeof(float2));
    buffers.Indices = (uint32_t*)malloc(buffers.IndexCount * sizeof(uint32_t));
    tric_triangulate(method, args.lod, &buffers);
    {
        VkDeviceMemory memory = 0;
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffers.VertexCount * sizeof(float2),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        };
        if (args.mode == Mode_Triangle_Cutout) {
            float2 *v = (float2*)buffers.Vertices;
            v[0] = (float2){   0.0f, 2.0f};
            v[1] = (float2){ ROOT_3,-1.0f};
            v[2] = (float2){-ROOT_3,-1.0f};
        }
        else if (args.mode == Mode_Quad_Cutout) {
            float2 *v = (float2*)buffers.Vertices;
            v[0] = (float2){-1.0f,-1.0f};
            v[1] = (float2){ 1.0f,-1.0f};
            v[2] = (float2){ 1.0f, 1.0f};
            v[3] = (float2){-1.0f, 1.0f};
        }
        tvkCreateBuffer(&context, &buffer_info, buffers.Vertices, TVK_MEMORY_USAGE_DEVICE, &vertex_buffer, &memory);
    }
    {
        VkDeviceMemory memory = 0;
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffers.IndexCount * sizeof(uint32_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        };
        tvkCreateBuffer(&context, &buffer_info, buffers.Indices, TVK_MEMORY_USAGE_DEVICE, &index_buffer, &memory);
    }
    free(buffers.Vertices);
    free(buffers.Indices);
    {
        VkDeviceMemory memory = 0;
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = instance_count * sizeof(instance_t),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        };
        if (args.single) instance_count = 1;
        instance_t *initial_data = (instance_t*)malloc(instance_count * sizeof(instance_t));
        if (args.single) {
            initial_data[0] = (instance_t){.offset = {0.0f, 0.0f}, .radius = 1.0f, .color = 0xFFFFFFFF};
        }
        else for (uint32_t i = 0; i < instance_count; ++i)
        {
            initial_data[i] = (instance_t){.offset = {randomf(), randomf()}, .radius = randomf() * 0.18f + 0.02f, .color = random_uint()};
			if (!args.use_blending) {
				initial_data[i].color |= 0xFF000000;
			}
		};
        tvkCreateBuffer(&context, &buffer_info, initial_data, TVK_MEMORY_USAGE_DEVICE, &instance_buffer, &memory);
        free(initial_data);
    }
    {
        VkDeviceMemory memory = 0;
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = instance_count * sizeof(float2),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        };
        float2 *initial_data = (float2*)malloc(instance_count * sizeof(float2));
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            float theta = randomf() * (6.28318530718f);
            initial_data[i] = (float2){cosf(theta), sinf(theta)};
        }
        tvkCreateBuffer(&context, &buffer_info, initial_data, TVK_MEMORY_USAGE_DEVICE, &velocity_buffer, &memory);
        free(initial_data);
    }
    assert(tvkSetBuilderAllocate(context.device, &set_builder, &set) == VK_SUCCESS);
    VkWriteDescriptorSet writes[] = {
        tvkSetBuilderWrite(&set_builder, set, 0, instance_buffer),
        tvkSetBuilderWrite(&set_builder, set, 1, velocity_buffer),
    };
    vkUpdateDescriptorSets(context.device, TVK_ARRAY_COUNT(writes), writes, 0, 0);

    VkQueryPoolCreateInfo query_pool_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    assert(vkCreateQueryPool(context.device, &query_pool_info, 0, &query_pool) == VK_SUCCESS);
}

void RecordCommandBuffers()
{
    assert(tvkAllocateCommandBuffers(context.device, context.command_pool,
        swap_chain.image_count, command_buffers) == VK_SUCCESS);

    VkBufferMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        .buffer = instance_buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    VkClearValue clear_value = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};
    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .renderArea = { {0, 0}, swap_chain.extent },
        .clearValueCount = 1,
        .pClearValues = &clear_value,
    };
    VkViewport viewport = {
        .width = (float)(swap_chain.extent.width),
        .height = (float)(swap_chain.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .extent = swap_chain.extent,
    };
    VkDeviceSize offsets[] = { 0u, 0u };
    VkBuffer buffers[] = { vertex_buffer, instance_buffer };

    for (uint32_t i = 0; i < swap_chain.image_count; ++i)
    {
        VkCommandBuffer cmd = command_buffers[i];
        begin_info.framebuffer = frame_buffers[i];
        assert(tvkBeginCommandBuffer(cmd, 0) == VK_SUCCESS);
        vkCmdResetQueryPool(cmd, query_pool, 0, 2);

        // Update Circles
        if (!args.single) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, c_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, c_pipeline_layout, 0, 1, &set, 0, 0);
            vkCmdDispatch(cmd, tvkCeilDiv(instance_count, 64), 1, 1);
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, 0, 1, &barrier, 0, 0);
        }

        // Draw Circles
        vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);
        vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0);
        vkCmdDrawIndexed(cmd, index_count, instance_count, 0, 0, 0);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1);
        vkCmdEndRenderPass(cmd);

        assert(vkEndCommandBuffer(cmd) == VK_SUCCESS);
    }
}
