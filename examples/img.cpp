#define TINY_VK_IMPLEMENTATION
#include "tinyvk.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main()
{
    TvkContext context = {};
    tvkCreateContext(TVK_CREATE_CONTEXT_ENABLE_VALIDATION, &context);

    TvkDescriptorSetBuilder builder = {};
    tvkSetBuilderAppend(&builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
    tvkSetBuilderCreate(context.device, VK_SHADER_STAGE_COMPUTE_BIT, 1, &builder);

    VkPipelineLayout pipeline_layout = 0;
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &builder.layout,
    };
    vkCreatePipelineLayout(context.device, &layout_info, 0, &pipeline_layout);

    VkShaderModule shader = 0;
    tvkCreateShaderFromFile(context.device, "image.spv", &shader);

    VkPipeline pipeline = 0;
    tvkCreateComputePipeline(context.device, pipeline_layout, shader, &pipeline);
    vkDestroyShaderModule(context.device, shader, 0);

    uint32_t N = 128;

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {.width = N, .height = N, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image = 0;
    VkDeviceMemory memory = 0;
    tvkCreateImage(&context, &image_info, TVK_MEMORY_USAGE_SHARED, &image, &memory);
    
    VkImageView view = 0;
    tvkCreateImageView(context.device, &image_info, image, &view);

    VkDescriptorSet set = 0;
    tvkSetBuilderAllocate(context.device, &builder, &set);

    VkWriteDescriptorSet writes[] = {
        tvkSetBuilderWrite(&builder, set, 0, view),
    };
    vkUpdateDescriptorSets(context.device, TVK_ARRAY_COUNT(writes), writes, 0, 0);

    VkCommandBuffer cmd = 0;
    tvkAllocateCommandBuffer(context.device, context.command_pool, 0, &cmd);

    tvkBeginCommandBuffer(cmd, 0);

    VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &image_barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, 0);
    vkCmdDispatch(cmd, tvkCeilDiv(N, 8), tvkCeilDiv(N, 8), 1);

    vkEndCommandBuffer(cmd);
    tvkQueueSumbitSingle(context.queue, cmd, 0);
    vkQueueWaitIdle(context.queue);

    float *gpu_data = 0;
    vkMapMemory(context.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&gpu_data);
    stbi_write_png("out.png", N, N, 4, gpu_data, N * sizeof(uint32_t));
    vkUnmapMemory(context.device, memory);

    tvkSetBuilderDestroy(context.device, &builder);
    vkFreeMemory(context.device, memory, 0);
    vkDestroyImageView(context.device, view, 0);
    vkDestroyImage(context.device, image, 0);
    vkDestroyPipeline(context.device, pipeline, 0);
    vkDestroyPipelineLayout(context.device, pipeline_layout, 0);
    tvkDestroyContext(&context);
}
