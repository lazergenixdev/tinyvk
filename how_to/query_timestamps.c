// https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html
#include "vulkan/vulkan.h"

VkDevice device;
VkPhysicalDevice physical_device;

int main()
{
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    // True if physical device supports timestamp queries
    properties.limits.timestampPeriod != 0.0f;

    // True if all Graphics & Compute queues support timestamp queries
    properties.limits.timestampComputeAndGraphics == VK_TRUE;

    // If previous was false, then we can check support for every individual queue
    if (properties.limits.timestampComputeAndGraphics == VK_FALSE)
    {
        // !! Here we should check all queue families !!
        VkQueueFamilyProperties queue_family_properties = {};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, 1, &queue_family_properties);

        // True if queue supports timestamp queries
        queue_family_properties.timestampValidBits != 0;
    }

    // Create Query pool
    VkQueryPool query_pool = 0;
    VkQueryPoolCreateInfo query_pool_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    vkCreateQueryPool(device, &query_pool_info, 0, &query_pool);

    VkCommandBuffer cmd = 0;

    // Before writing to query pool, it must be reset
    vkCmdResetQueryPool(cmd, query_pool, 0, 2);

    // Example usage of writing timestamps
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1);

    // Collect timestamps (after vkQueueSubmit)
    uint64_t timestamps[2];
    vkGetQueryPoolResults(
        device, query_pool, 0, 2, sizeof(timestamps),
        &timestamps, sizeof(timestamps[0]),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );

    // Interpret results
    float delta_in_ms = (float)(timestamps[1] - timestamps[0])
                      * properties.limits.timestampPeriod / 1e6f;
    printf("GPU took %f ms\n", delta_in_ms);
}
