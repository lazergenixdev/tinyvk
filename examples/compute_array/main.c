#define TINY_VK_IMPLEMENTATION
#include "tinyvk.h"

int main()
{
    TvkContext context = {};
    tvkCreateContext(TVK_CREATE_CONTEXT_ENABLE_VALIDATION, &context);

    TvkDescriptorSetBuilder builder = {};
    tvkSetBuilderAppend(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    tvkSetBuilderCreate(context.device, VK_SHADER_STAGE_COMPUTE_BIT, 1, &builder);

    VkPipelineLayout pipeline_layout = 0;
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &builder.layout,
    };
    vkCreatePipelineLayout(context.device, &layout_info, 0, &pipeline_layout);

    VkShaderModule shader = 0;
    tvkCreateShaderFromFile(context.device, "kernel.spv", &shader);

    VkPipeline pipeline = 0;
    tvkCreateComputePipeline(context.device, pipeline_layout, shader, &pipeline);
    vkDestroyShaderModule(context.device, shader, 0);

    int N = 128;
    float *numbers = (float*)malloc(N * sizeof(float));
    for (int i = 0; i < N; ++i)
        numbers[i] = (float)(i);

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = N * sizeof(float),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };
    VkBuffer buffer = 0;
    VkDeviceMemory memory = 0;
    tvkCreateBuffer(&context, &buffer_info, numbers, TVK_MEMORY_USAGE_SHARED, &buffer, &memory);
    
    VkDescriptorSet set = 0;
    tvkSetBuilderAllocate(context.device, &builder, &set);

    VkWriteDescriptorSet writes[] = {
        tvkSetBuilderWrite(&builder, set, 0, buffer),
    };
    vkUpdateDescriptorSets(context.device, TVK_ARRAY_COUNT(writes), writes, 0, 0);

    VkCommandBuffer cmd = 0;
    tvkAllocateCommandBuffer(context.device, context.command_pool, 0, &cmd);

    tvkBeginCommandBuffer(cmd, 0);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, 0);
    vkCmdDispatch(cmd, tvkCeilDiv(N, 64), 1, 1);

    vkEndCommandBuffer(cmd);
    tvkQueueSumbitSingle(context.queue, cmd, 0);
    vkQueueWaitIdle(context.queue);

    float *gpu_data = 0;
    vkMapMemory(context.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&gpu_data);
    for (int i = 0; i < N; ++i) {
        float error = gpu_data[i] - (float)(i * i);
        printf("%3i: %8.1f (%s)\n", i, gpu_data[i], ((error == 0.0f)?"PASS":"FAIL"));
    }
    vkUnmapMemory(context.device, memory);

    tvkSetBuilderDestroy(context.device, &builder);
    vkFreeMemory(context.device, memory, 0);
    vkDestroyBuffer(context.device, buffer, 0);
    vkDestroyPipeline(context.device, pipeline, 0);
    vkDestroyPipelineLayout(context.device, pipeline_layout, 0);
    tvkDestroyContext(&context);
}
