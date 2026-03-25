/* tinyvk.h - 2026.03.12 - Public Domain - https://github.com/lazergenixdev/tinyvk.h

   Do this:
      #define TINY_VK_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

 * tinyvk is a single-header helper library of "starter code" for Vulkan
 * tinyvk handles all the initial annoying setup to get you started fast
 * tinyvk is NOT an abstraction layer, it does not seek to hide Vulkan details from you

IDEA
  1. You include this in your project to get started.
  2. Modify the file on a need-to-have basis for features/functionality.
  3. Eventually, over time, copy all the functionality directy into your engine.

LICENSE

  See end of file for license information.

*/

#ifndef TINY_VK_H
#define TINY_VK_H

#ifndef TVK_APPLICATION_NAME
#define TVK_APPLICATION_NAME "Vulkan App"
#endif

#ifndef TVK_ENGINE_NAME
#define TVK_ENGINE_NAME "Engine Name"
#endif

#ifndef TVK_INSTANCE_EXTENSIONS
#define TVK_INSTANCE_EXTENSIONS
#endif

#ifndef TVK_DEVICE_EXTENSIONS
#define TVK_DEVICE_EXTENSIONS
#endif

#ifndef TVK_MAX_SWAP_CHAIN_IMAGES
#define TVK_MAX_SWAP_CHAIN_IMAGES 16
#endif

#ifndef TVKDEF
/*
   Goes before declarations and definitions of the nob functions. Useful to `#define TVKDEF static inline`
   if your source code is a single file and you want the compiler to remove unused functions.
*/
#define TVKDEF
#endif

#ifndef TVK_REALLOC
#define TVK_REALLOC realloc
#endif

#ifndef TVK_FREE
#define TVK_FREE free
#endif

#ifndef TVK_TEMP_ALLOC
#define TVK_TEMP_ALLOC(T,N) (T*)alloca(N * sizeof(T))
#endif

#ifndef TVK_TEMP_FREE
#define TVK_TEMP_FREE(P) (void)(P)
#endif

#define TVK_TRY(F) if ((result = (F)) != VK_SUCCESS) return result
#define TVK_ARRAY_COUNT(A) (uint32_t)(sizeof(A)/sizeof(A[0]))

#include "vulkan/vulkan.h"
#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "stdbool.h"
#include "memory.h"

enum {
    TVK_MEMORY_USAGE_SHARED = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    TVK_MEMORY_USAGE_HOST   = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    TVK_MEMORY_USAGE_DEVICE = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    TVK_CREATE_CONTEXT_ENABLE_VALIDATION = 0x01,
};
typedef uint32_t TvkFlags;

typedef struct {
    uint32_t graphics;
    uint32_t present;
    uint32_t transfer;
} TvkQueueFamilies;

typedef struct {
    VkInstance       instance;
    VkPhysicalDevice physical_device;
    VkDevice         device;
    TvkQueueFamilies queue_families;
    VkQueue          queue;          /* General purpose queue */
    VkQueue          transfer_queue; /* Queue specialized for transfers (equal to queue if none exists) */
    VkQueue          present_queue;  /* Queue specialized for presenting (equal to queue if none exists) */
    VkCommandPool    command_pool;
} TvkContext;

typedef struct {
    VkSurfaceKHR   surface;
    VkSwapchainKHR swap_chain;
    VkImage        images[TVK_MAX_SWAP_CHAIN_IMAGES];
    VkImageView    image_views[TVK_MAX_SWAP_CHAIN_IMAGES];
    uint32_t       image_count;
    VkFormat       format;
    VkExtent2D     extent;
} TvkSwapChain;

typedef struct {
    VkDescriptorType type;
    uint32_t         count;
} TvkDescriptor;

typedef union {
    VkDescriptorBufferInfo bufferInfo;
    VkDescriptorImageInfo  imageInfo;
} TvkDescriptorInfo;

typedef struct {
    VkDescriptorPool       pool;
    VkDescriptorSetLayout  layout;
    TvkDescriptor*         descriptors;
    TvkDescriptorInfo*     infos;
} TvkDescriptorSetBuilder;

TVKDEF VkResult tvkCreateContext(TvkFlags flags, TvkContext *context);
TVKDEF void tvkDestroyContext(TvkContext* context);

TVKDEF VkResult tvkSetBuilderCreate(VkDevice device, VkShaderStageFlags stages, uint32_t max_sets, TvkDescriptorSetBuilder *block);
TVKDEF void tvkSetBuilderDestroy(VkDevice device, TvkDescriptorSetBuilder *builder);
TVKDEF void tvkSetBuilderAppend(TvkDescriptorSetBuilder *builder, VkDescriptorType type, uint32_t count);
TVKDEF VkResult tvkSetBuilderAllocate(VkDevice device, TvkDescriptorSetBuilder *builder, VkDescriptorSet *set);
TVKDEF VkWriteDescriptorSet tvkSetBuilderWrite(TvkDescriptorSetBuilder *builder, VkDescriptorSet set, uint32_t binding, void *object);

TVKDEF VkFence  tvkCreateFence(VkDevice device, VkFlags flags);
TVKDEF VkResult tvkBeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags flags);
TVKDEF VkResult tvkCreateCommandPool(VkDevice device, VkFlags flags, VkCommandPool *pool);
TVKDEF VkResult tvkQueueSumbitSingle(VkQueue queue, VkCommandBuffer buffer, VkFence fence);
TVKDEF VkResult tvkCreateShaderFromFile(VkDevice device, const char* filename, VkShaderModule *shader);
TVKDEF VkResult tvkCreateShaderFromMemory(VkDevice device, void *data, size_t size, VkShaderModule *shader);
TVKDEF uint32_t tvkFindMemoryType(VkPhysicalDevice physical_device, uint32_t mask, VkFlags required_properties);
TVKDEF VkResult tvkAllocateCommandBuffer(VkDevice device, VkCommandPool pool, VkCommandBuffer *command_buffer);
TVKDEF VkResult tvkCreateSingleSetLayout(VkDevice device, VkDescriptorType type, VkShaderStageFlags stages, VkDescriptorSetLayout *layout);
TVKDEF VkResult tvkCreateComputePipeline(VkDevice device, VkPipelineLayout layout, VkShaderModule shader, VkPipeline *pipeline);

/* Ideally you would have your own arena-style allocator,
   but this interface is provided to get started */

TVKDEF VkResult tvkCreateBuffer(TvkContext *context, VkBufferCreateInfo *buffer_info, void *initial_data, TvkFlags memory_usage, VkBuffer *buffer, VkDeviceMemory *memory);
TVKDEF VkResult tvkCreateImage(TvkContext *context, VkImageCreateInfo *image_info, TvkFlags memory_usage, VkImage *image, VkDeviceMemory *memory);
TVKDEF VkResult tvkCreateImageView(VkDevice device, VkImageCreateInfo *image_info, VkImage image, VkImageView *view);

TVKDEF VkResult tvkCreateInstance(bool enable_validation, VkInstance *instance);
TVKDEF VkResult tvkPickPhysicalDevice(VkInstance instance, VkPhysicalDevice *physical_device);
TVKDEF VkResult tvkCreateDevice(VkPhysicalDevice physical_device, TvkQueueFamilies queue_families, VkDevice *device);
TVKDEF uint32_t tvkAppendUniqueQueueFamily(VkDeviceQueueCreateInfo *infos, uint32_t count, uint32_t family_index, float* priority);

static inline uint32_t tvkCeilDiv(uint32_t num, uint32_t den)
{
    return (num + den - 1) / den;
}

static inline uint64_t tvkAlign(uint64_t address, uint64_t alignment)
{
    return (address + alignment - 1) & ~(alignment - 1);
}

#if 0 // TODO: deal with queue families properly!
{
    VkQueueFamilyProperties properties;
    VkPhysicalDevice d;
    VkSurfaceKHR surface;
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(d, &count, 0);
    properties.queueFlags;
    VK_QUEUE_GRAPHICS_BIT;
    VK_QUEUE_COMPUTE_BIT;
    VK_QUEUE_TRANSFER_BIT;
    VkBool32 supports_present = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(d, 1, surface, &supports_present);
}
#endif

#endif // TINY_VK_H

#ifdef TINY_VK_IMPLEMENTATION

typedef struct {
    uint32_t count;
    uint32_t capacity;
} Tvk__ArrayMetadata;

#define TVK__ALLOCATE(T,N) (T*)TVK_REALLOC(0, (N)*sizeof(T))
#define TVK__ARRAY_META(A) ((Tvk__ArrayMetadata*)(A))[-1]
#define TVK__ARRAY_COUNT(A) (TVK__ARRAY_META(A).count)
#define TVK__ARRAY_APPEND(A, ...) (tvk__array_grow_by_one((void**)&(A), sizeof(A[0])), (A)[TVK__ARRAY_META(A).count++] = __VA_ARGS__)
#define TVK__ARRAY_CREATE(T,N) (T*)tvk__array_create(sizeof(T), N)

static inline void tvk__array_grow_by_one(void **parr, uint32_t elem_size)
{
    void *arr = *parr;
    uint32_t prev_capacity = 0;
    uint32_t prev_count = 0;
    void *ptr = 0;
    if (arr) {
        Tvk__ArrayMetadata meta = TVK__ARRAY_META(arr);
        if (meta.capacity >= meta.count + 1)
            return;
        prev_capacity = meta.capacity;
        prev_count = meta.count;
        ptr = (void*)((char*)(arr) - sizeof(Tvk__ArrayMetadata));
    }
    uint32_t new_capacity = (prev_capacity + 4) * 3 / 2;
    void *new_arr = TVK_REALLOC(ptr, (new_capacity * elem_size) + sizeof(Tvk__ArrayMetadata));
    new_arr = (char*)(new_arr) + sizeof(Tvk__ArrayMetadata);
    TVK__ARRAY_META(new_arr).capacity = new_capacity;
    TVK__ARRAY_META(new_arr).count = prev_count;
    *parr = new_arr;
}

static inline void *tvk__array_create(uint32_t elem_size, uint32_t count)
{
    uint32_t shift = sizeof(Tvk__ArrayMetadata);
    void* arr = TVK_REALLOC(0, count * elem_size + shift);
    arr = (char*)(arr) + sizeof(Tvk__ArrayMetadata);
    TVK__ARRAY_META(arr).capacity = count;
    TVK__ARRAY_META(arr).count = count;
    return arr;
}

TVKDEF VkResult tvkCreateInstance(bool enable_validation, VkInstance *instance)
{
    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = TVK_APPLICATION_NAME,
        .applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 1),
        .pEngineName = TVK_ENGINE_NAME,
        .engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 1),
        .apiVersion = VK_API_VERSION_1_4, /* NOTE: Vulkan API version selected here! */
    };
    const char* extensions[] = {
	#if defined(__APPLE__)
		VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
	#endif
        TVK_INSTANCE_EXTENSIONS
    };
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = 0, /* TODO: Add ability to append features */
    #if defined(__APPLE__)
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
    #endif
        .pApplicationInfo = &application_info,
        .enabledLayerCount = enable_validation ? TVK_ARRAY_COUNT(layers) : 0u,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = TVK_ARRAY_COUNT(extensions),
        .ppEnabledExtensionNames = extensions,
    };
    return vkCreateInstance(&instance_info, 0, instance);
}

/* NOTE: This code assumes that any GPU will be suitable and can be picked,
         but this assumption is not always true!  */
TVKDEF VkResult tvkPickPhysicalDevice(VkInstance instance, VkPhysicalDevice *physical_device)
{
    VkResult result = VK_SUCCESS;
    uint32_t physical_device_count = 0;
    TVK_TRY(vkEnumeratePhysicalDevices(instance, &physical_device_count, 0));

    if (physical_device_count == 0)
        return VK_ERROR_UNKNOWN;

    VkPhysicalDevice *physical_devices = TVK_TEMP_ALLOC(VkPhysicalDevice, physical_device_count);
    TVK_TRY(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices));

    uint32_t picked_index = 0;
    for (uint32_t i = 0; i < physical_device_count; ++i)
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(physical_devices[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            picked_index = i;
            break;
        }
    }
    TVK_TEMP_FREE(physical_devices);
    *physical_device = physical_devices[picked_index];
    return VK_SUCCESS;
}

TVKDEF uint32_t tvkAppendUniqueQueueFamily(VkDeviceQueueCreateInfo *infos, uint32_t count, uint32_t family_index, float* priority)
{
    for (uint32_t i = 0; i < count; ++i) {
        if (infos[i].queueFamilyIndex == family_index)
            return 0;
    }
    infos[count] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family_index,
        .queueCount = 1,
        .pQueuePriorities = priority,
    };
    return 1;
}

TVKDEF VkResult tvkCreateDevice(VkPhysicalDevice physical_device, TvkQueueFamilies queue_families, VkDevice *device)
{
    VkResult result = VK_SUCCESS;
    float priority = 1.0f;
    uint32_t queue_count = 0;
    VkDeviceQueueCreateInfo queue_infos[3];

    queue_count += tvkAppendUniqueQueueFamily(
        queue_infos, queue_count, queue_families.graphics, &priority);
    queue_count += tvkAppendUniqueQueueFamily(
        queue_infos, queue_count, queue_families.present, &priority);
    queue_count += tvkAppendUniqueQueueFamily(
        queue_infos, queue_count, queue_families.transfer, &priority);

    const char* extensions[] = {
#if defined(__APPLE__)
        "VK_KHR_portability_subset",
#endif
        TVK_DEVICE_EXTENSIONS
    };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = 0, /* TODO: Device features */
        .queueCreateInfoCount = queue_count,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount = TVK_ARRAY_COUNT(extensions),
        .ppEnabledExtensionNames = extensions,
    };
    TVK_TRY(vkCreateDevice(physical_device, &device_info, 0, device));

    return VK_SUCCESS;
}

TVKDEF VkResult tvkCreateSingleSetLayout(VkDevice device, VkDescriptorType type, VkShaderStageFlags stages, VkDescriptorSetLayout *layout)
{
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stages,
        .pImmutableSamplers = 0,
    };
    VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    return vkCreateDescriptorSetLayout(device, &set_layout_info, 0, layout);
}

TVKDEF VkResult tvkCreateShaderFromFile(VkDevice device, const char* filename, VkShaderModule *shader)
{
    FILE *f = fopen(filename, "rb");
    if (!f) return VK_ERROR_UNKNOWN;
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *data = TVK_REALLOC(0, size);
    size_t bytes_read = fread(data, 1, size, f);
    assert(bytes_read == size);
    fclose(f);

    VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (uint32_t*)(data),
    };
    VkResult result = vkCreateShaderModule(device, &shader_info, 0, shader);
    TVK_FREE(data);
    return result;
}

TVKDEF VkResult tvkCreateShaderFromMemory(VkDevice device, void *data, size_t size, VkShaderModule *shader)
{
    VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (uint32_t*)(data),
    };
    return vkCreateShaderModule(device, &shader_info, 0, shader);
}

TVKDEF VkResult tvkCreateComputePipeline(VkDevice device, VkPipelineLayout layout, VkShaderModule shader, VkPipeline *pipeline)
{
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main",
        },
        .layout = layout,
    };
    return vkCreateComputePipelines(device, 0, 1, &pipeline_info, 0, pipeline);
}

TVKDEF VkFence tvkCreateFence(VkDevice device, VkFlags flags)
{
    VkFence fence = 0;
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags,
    };
    if (vkCreateFence(device, &fence_info, 0, &fence) != VK_SUCCESS)
        return 0;
    return fence;
}

TVKDEF VkResult tvkAllocateCommandBuffer(VkDevice device, VkCommandPool pool, VkCommandBuffer *command_buffer)
{
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    return vkAllocateCommandBuffers(device, &allocate_info, command_buffer);
}

TVKDEF VkResult tvkBeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
    return vkBeginCommandBuffer(cmd, &begin_info);
}

TVKDEF uint32_t tvkFindMemoryType(VkPhysicalDevice physical_device, uint32_t mask, VkFlags required_properties)
{
	VkPhysicalDeviceMemoryProperties properties = {};
	vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
	uint32_t memory_type = VK_MAX_MEMORY_TYPES;
	for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
		if (!(mask & (1 << i)))
			continue; // Skip memory type if we don't support it in our mask
		if ((properties.memoryTypes[i].propertyFlags & required_properties) != required_properties)
			continue; // Also skip memory type if it doesn't have the properties we want
		memory_type = i;
	}
	return memory_type;
}

TVKDEF VkResult tvkCreateBuffer(TvkContext *context, VkBufferCreateInfo *buffer_info, void *initial_data,
                                TvkFlags memory_usage, VkBuffer *buffer, VkDeviceMemory *memory)
{
    VkResult result = VK_SUCCESS;
    TVK_TRY(vkCreateBuffer(context->device, buffer_info, 0, buffer));
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(context->device, *buffer, &requirements);
    VkMemoryPropertyFlags properties = memory_usage;
    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = tvkFindMemoryType(context->physical_device, requirements.memoryTypeBits, properties),
    };
    TVK_TRY(vkAllocateMemory(context->device, &allocate_info, 0, memory));
    TVK_TRY(vkBindBufferMemory(context->device, *buffer, *memory, 0));

    if (initial_data)
    {
        if (memory_usage == TVK_MEMORY_USAGE_HOST || memory_usage == TVK_MEMORY_USAGE_SHARED)
        {
            float *gpu_data = 0;
            vkMapMemory(context->device, *memory, 0, VK_WHOLE_SIZE, 0, (void**)&gpu_data);
            memcpy(gpu_data, initial_data, buffer_info->size);
            vkUnmapMemory(context->device, *memory);
        }
    }
    return VK_SUCCESS;
}

TVKDEF VkResult tvkCreateImage(TvkContext *context, VkImageCreateInfo *image_info, TvkFlags memory_usage, VkImage *image, VkDeviceMemory *memory)
{
    VkResult result = VK_SUCCESS;
    TVK_TRY(vkCreateImage(context->device, image_info, 0, image));
    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(context->device, *image, &requirements);
    VkMemoryPropertyFlags properties = memory_usage;
    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = tvkFindMemoryType(context->physical_device, requirements.memoryTypeBits, properties),
    };
    TVK_TRY(vkAllocateMemory(context->device, &allocate_info, 0, memory));
    TVK_TRY(vkBindImageMemory(context->device, *image, *memory, 0));
    return VK_SUCCESS;
}

TVKDEF VkResult tvkCreateImageView(VkDevice device, VkImageCreateInfo *image_info, VkImage image, VkImageView *view)
{
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, // TODO: fix
        .format = image_info->format,
        .components = {},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, // TODO: fix
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    return vkCreateImageView(device, &view_info, 0, view);
}

TVKDEF VkResult tvkQueueSumbitSingle(VkQueue queue, VkCommandBuffer command_buffer, VkFence fence)
{
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    return vkQueueSubmit(queue, 1, &submit, fence);
}

TVKDEF VkResult tvkCreateContext(TvkFlags flags, TvkContext *context)
{
    VkResult result = VK_SUCCESS;
    bool enable_validation = (bool)(flags & TVK_CREATE_CONTEXT_ENABLE_VALIDATION);

    TVK_TRY(tvkCreateInstance(enable_validation, &context->instance));
    TVK_TRY(tvkPickPhysicalDevice(context->instance, &context->physical_device));

    TvkQueueFamilies families = {};
    TVK_TRY(tvkCreateDevice(context->physical_device, families, &context->device));

    vkGetDeviceQueue(context->device, context->queue_families.graphics, 0, &context->queue);
    vkGetDeviceQueue(context->device, context->queue_families.present, 0, &context->present_queue);
    vkGetDeviceQueue(context->device, context->queue_families.transfer, 0, &context->transfer_queue);

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    TVK_TRY(vkCreateCommandPool(context->device, &pool_info, 0, &context->command_pool));

    return result;
}

TVKDEF void tvkDestroyContext(TvkContext* context)
{
    if (context->command_pool) vkDestroyCommandPool(context->device, context->command_pool, 0);
    if (context->device) vkDestroyDevice(context->device, 0);
    if (context->instance) vkDestroyInstance(context->instance, 0);
    memset(context, 0, sizeof(TvkContext));
}

TVKDEF void tvkSetBuilderAppend(TvkDescriptorSetBuilder *builder, VkDescriptorType type, uint32_t count)
{
    assert(builder->layout == VK_NULL_HANDLE);
    TVK__ARRAY_APPEND(builder->descriptors, (TvkDescriptor){.type = type, .count = count});
}

TVKDEF VkResult tvkSetBuilderCreate(VkDevice device, VkShaderStageFlags stages, uint32_t max_sets, TvkDescriptorSetBuilder *builder)
{
    VkResult result = VK_SUCCESS;
    uint32_t binding_count = TVK__ARRAY_COUNT(builder->descriptors);
    VkDescriptorSetLayoutBinding *bindings = TVK__ALLOCATE(VkDescriptorSetLayoutBinding, binding_count);
    for (uint32_t i = 0; i < binding_count; ++i) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = i,
            .descriptorType = builder->descriptors[i].type,
            .descriptorCount = builder->descriptors[i].count,
            .stageFlags = stages,
        };
    };
    VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = binding_count,
        .pBindings = bindings,
    };
    // NOTE: Here we are reusing the memory allocated for the descriptor set bindings
    VkDescriptorPoolSize *pool_sizes = (VkDescriptorPoolSize*)(bindings);
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = max_sets,
        .poolSizeCount = 0,
        .pPoolSizes = pool_sizes,
    };

    if ((result = vkCreateDescriptorSetLayout(device, &set_layout_info, 0, &builder->layout)) != VK_SUCCESS)
        goto error;

    for (uint32_t i = 0; i < binding_count; ++i) {
        TvkDescriptor descriptor = builder->descriptors[i];
        bool duplicate = false;
        for (uint32_t j = 0; j < i; ++i) {
            if (builder->descriptors[j].type == descriptor.type) {
                pool_sizes[j].descriptorCount += descriptor.count * max_sets;
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        pool_sizes[pool_info.poolSizeCount++] = (VkDescriptorPoolSize){
            .type = descriptor.type,
            .descriptorCount = descriptor.count * max_sets,
        };
    };

    if ((result = vkCreateDescriptorPool(device, &pool_info, 0, &builder->pool)) != VK_SUCCESS)
        goto error;

    builder->infos = TVK__ARRAY_CREATE(TvkDescriptorInfo, binding_count);

error:
    TVK_FREE(bindings);
    return result;
}

TVKDEF void tvkSetBuilderDestroy(VkDevice device, TvkDescriptorSetBuilder *builder)
{
    if (builder->pool) vkDestroyDescriptorPool(device, builder->pool, 0);
    if (builder->layout) vkDestroyDescriptorSetLayout(device, builder->layout, 0);
}

TVKDEF VkResult tvkSetBuilderAllocate(VkDevice device, TvkDescriptorSetBuilder *builder, VkDescriptorSet *set)
{
    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = builder->pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &builder->layout,
    };
    return vkAllocateDescriptorSets(device, &allocate_info, set);
}

TVKDEF VkWriteDescriptorSet tvkSetBuilderWrite(TvkDescriptorSetBuilder *builder, VkDescriptorSet set, uint32_t binding, void *object)
{
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = builder->descriptors[binding].count,
        .descriptorType = builder->descriptors[binding].type,
    };
    if (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    ||  write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
        builder->infos[binding].bufferInfo = (VkDescriptorBufferInfo){
            .buffer = (VkBuffer)(object),
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };
        write.pBufferInfo = &builder->infos[binding].bufferInfo;
    }
    else if (write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
         ||  write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        builder->infos[binding].imageInfo = (VkDescriptorImageInfo){
            .sampler = 0,
            .imageView = (VkImageView)(object),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        write.pImageInfo = &builder->infos[binding].imageInfo;
    }
    else {
        assert(false);
    }
    return write;
}

#endif // TINY_VK_IMPLEMENTATION

/* ----------------------------------------------------------------------------
    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.

    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.

    For more information, please refer to <https://unlicense.org/>
---------------------------------------------------------------------------- */
