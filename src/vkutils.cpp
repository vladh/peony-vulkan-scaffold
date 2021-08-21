/*
  Functions to assist with Vulkan tasks such as
  (1) creating and copying buffers and images,
  (2) creating basic Vulkan structs more easily.

  These functions should not rely on VkState, but rather only on Vulkan types.
*/


namespace vkutils {
  /////////////////////////
  // Struct creation utils
  /////////////////////////
  VkSamplerCreateInfo sampler_create_info(
    VkPhysicalDeviceProperties physical_device_props
  ) {
    return {
      .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter               = VK_FILTER_LINEAR,
      .minFilter               = VK_FILTER_LINEAR,
      .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias              = 0.0f,
      .anisotropyEnable        = VK_TRUE,
      .maxAnisotropy           =
        physical_device_props.limits.maxSamplerAnisotropy,
      .compareEnable           = VK_FALSE,
      .compareOp               = VK_COMPARE_OP_ALWAYS,
      .minLod                  = 0.0f,
      .maxLod                  = 0.0f,
      .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
    };
  }


  VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state() {
    return {
      .blendEnable         = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp        = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp        = VK_BLEND_OP_ADD,
      .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT,
    };
  }


  VkAttachmentReference attachment_reference(
    u32 attachment, VkImageLayout layout
  ) {
    return {
      .attachment = attachment,
      .layout     = layout,
    };
  }


  VkAttachmentDescription attachment_description(
    VkFormat format, VkImageLayout finalLayout
  ) {
    return {
      .format         = format,
      .samples        = VK_SAMPLE_COUNT_1_BIT,
      .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout    = finalLayout,
    };
  }


  VkCommandBufferBeginInfo command_buffer_begin_info() {
    return {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
  }


  VkCommandBufferAllocateInfo command_buffer_allocate_info(
    VkCommandPool commandPool
  ) {
    return {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = commandPool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
  }


  VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info(
    const VkViewport* pViewports,
    const VkRect2D* pScissors
  ) {
    return {
      .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports    = pViewports,
      .scissorCount  = 1,
      .pScissors     = pScissors,
    };
  }


  VkPipelineLayoutCreateInfo pipeline_layout_create_info(
    const VkDescriptorSetLayout* pSetLayouts
  ) {
    return {
      .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts    = pSetLayouts,
    };
  }


  VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info_vert(
    VkShaderModule shader_module
  ) {
    return {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_VERTEX_BIT,
      .module = shader_module,
      .pName  = "main",
    };
  }


  VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info_frag(
    VkShaderModule shader_module
  ) {
    return {
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = shader_module,
      .pName  = "main",
    };
  }


  VkDescriptorSetAllocateInfo descriptor_set_allocate_info(
    VkDescriptorPool descriptorPool,
    const VkDescriptorSetLayout* pSetLayouts
  ) {
    return {
      .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool     = descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts        = pSetLayouts,
    };
  }


  VkWriteDescriptorSet write_descriptor_set_buffer(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    const VkDescriptorBufferInfo* pBufferInfo
  ) {
    return {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = dstSet,
      .dstBinding      = dstBinding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo     = pBufferInfo,
    };
  }


  VkWriteDescriptorSet write_descriptor_set_image(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    const VkDescriptorImageInfo* pImageInfo
  ) {
    return {
      .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet          = dstSet,
      .dstBinding      = dstBinding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo      = pImageInfo,
    };
  }


  VkDescriptorPoolSize descriptor_pool_size(
    VkDescriptorType type,
    u32 descriptorCount
  ) {
    return {
      .type            = type,
      .descriptorCount = descriptorCount,
    };
  }


  VkDescriptorPoolCreateInfo descriptor_pool_create_info(
    u32 maxSets,
    u32 poolSizeCount,
    const VkDescriptorPoolSize* pPoolSizes
  ) {
    return {
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = maxSets,
      .poolSizeCount = poolSizeCount,
      .pPoolSizes    = pPoolSizes,
    };
  }


  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info(
    u32 bindingCount,
    const VkDescriptorSetLayoutBinding* pBindings
  ) {
    return {
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = bindingCount,
      .pBindings    = pBindings,
    };
  }


  VkDescriptorSetLayoutBinding descriptor_set_layout_binding(
    u32 binding,
    VkDescriptorType descriptorType
  ) {
    return {
      .binding         = binding,
      .descriptorType  = descriptorType,
      .descriptorCount = 1,
      .stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS,
    };
  }


  VkDescriptorSetLayoutBinding descriptor_set_layout_binding(
    u32 binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags
  ) {
    return {
      .binding         = binding,
      .descriptorType  = descriptorType,
      .descriptorCount = 1,
      .stageFlags      = stageFlags,
    };
  }

  /////////////////////////
  // General utils
  /////////////////////////
  void check(VkResult result) {
    assert(result == VK_SUCCESS);
  }

  void create_semaphore(VkDevice device, VkSemaphore *semaphore) {
    VkSemaphoreCreateInfo const semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    check(vkCreateSemaphore(device, &semaphore_info, nullptr, semaphore));
  }

  u32 find_memory_type(
    VkPhysicalDevice physical_device, u32 type_filter,
    VkMemoryPropertyFlags desired_properties
  ) {
    VkPhysicalDeviceMemoryProperties actual_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &actual_properties);

    range (0, actual_properties.memoryTypeCount) {
      if (
        type_filter & (1 << idx) &&
        actual_properties.memoryTypes[idx].propertyFlags & desired_properties
      ) {
        return idx;
      }
    }

    logs::fatal("Could not find suitable memory type.");
    return 0;
  }


  VkCommandBuffer begin_command_buffer(
    VkDevice device,
    VkCommandPool command_pool
  ) {
    VkCommandBufferAllocateInfo const alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo const begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
  }


  void end_command_buffer(
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    VkCommandBuffer command_buffer
  ) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo const submit_info = {
      .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers    = &command_buffer,
    };
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
  }


  void create_buffer(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer *buffer,
    VkDeviceMemory *memory
  ) {
    VkBufferCreateInfo const buffer_info = {
      .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size        = size,
      .usage       = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    check(vkCreateBuffer(device, &buffer_info, nullptr, buffer));

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, *buffer, &requirements);

    u32 memory_type = find_memory_type(physical_device,
      requirements.memoryTypeBits, properties);
    VkMemoryAllocateInfo const alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = requirements.size,
      .memoryTypeIndex = memory_type,
    };

    check(vkAllocateMemory(device, &alloc_info, nullptr, memory));

    vkBindBufferMemory(device, *buffer, *memory, 0);
  }


  void copy_buffer(
    VkDevice device,
    VkCommandPool command_pool,
    VkQueue queue,
    VkBuffer src, VkBuffer dest, VkDeviceSize size
  ) {
    VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);
    VkBufferCopy copy_region = {.size = size};
    vkCmdCopyBuffer(command_buffer, src, dest, 1, &copy_region);
    end_command_buffer(device, queue, command_pool, command_buffer);
  }


  void create_image(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkImage *image,
    VkDeviceMemory *image_memory,
    u32 width, u32 height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties
  ) {
    // Create VkImage
    VkImageCreateInfo const image_info = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = format,
      .extent = {
        .width       = width,
        .height      = height,
        .depth       = 1,
      },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = tiling,
      .usage         = usage,
      .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    check(vkCreateImage(device, &image_info, nullptr, image));

    // Allocate memory
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, *image, &requirements);
    VkMemoryAllocateInfo const alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = requirements.size,
      .memoryTypeIndex = find_memory_type(physical_device,
        requirements.memoryTypeBits, properties),
    };
    check(vkAllocateMemory(device, &alloc_info, nullptr, image_memory));
    vkBindImageMemory(device, *image, *image_memory, 0);
  }


  VkImageView create_image_view(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags
  ) {
    VkImageViewCreateInfo const image_view_info = {
      .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image            = image,
      .viewType         = VK_IMAGE_VIEW_TYPE_2D,
      .format           = format,
      .subresourceRange = {
        .aspectMask     = aspect_flags,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
    };
    VkImageView image_view;
    check(vkCreateImageView(device, &image_view_info, nullptr,
      &image_view));
    return image_view;
  }


  void create_image_resources(
    ImageResources *image_resources,
    VkDevice device,
    VkPhysicalDevice physical_device,
    u32 width, u32 height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImageAspectFlags aspect_flags
  ) {
    create_image(device, physical_device,
      &image_resources->image, &image_resources->memory,
      width, height,
      format, tiling, usage, properties);
    image_resources->view = create_image_view(device, image_resources->image,
      format, aspect_flags);
  }


  void create_image_resources_with_sampler(
    ImageResources *image_resources,
    VkDevice device,
    VkPhysicalDevice physical_device,
    u32 width, u32 height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImageAspectFlags aspect_flags,
    VkPhysicalDeviceProperties physical_device_properties
  ) {
    create_image_resources(
      image_resources,
      device,
      physical_device,
      width, height,
      format,
      tiling,
      usage,
      properties,
      aspect_flags
    );
    VkSamplerCreateInfo const sampler_info = sampler_create_info(
      physical_device_properties);
    check(vkCreateSampler(device, &sampler_info, nullptr,
      &image_resources->sampler));
  }


  void destroy_image_resources(
    ImageResources *image_resources, VkDevice device
  ) {
    vkDestroyImageView(device, image_resources->view, nullptr);
    vkDestroyImage(device, image_resources->image, nullptr);
    vkFreeMemory(device, image_resources->memory, nullptr);
  }


  void destroy_image_resources_with_sampler(
    ImageResources *image_resources, VkDevice device
  ) {
    destroy_image_resources(image_resources, device);
    vkDestroySampler(device, image_resources->sampler, nullptr);
  }


  void transition_image_layout(
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    VkImage image,
    VkFormat format,
    VkImageLayout old_layout,
    VkImageLayout new_layout
  ) {
    VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);

    VkImageMemoryBarrier barrier = {
      .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask       = 0, // Filled in later
      .dstAccessMask       = 0, // Filled in later
      .oldLayout           = old_layout,
      .newLayout           = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image               = image,
      .subresourceRange = {
        .aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel      = 0,
        .levelCount        = 1,
        .baseArrayLayer    = 0,
        .layerCount        = 1,
      },
    };

    VkPipelineStageFlags source_stage = {};
    VkPipelineStageFlags destination_stage = {};

    if (
      old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      source_stage          = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destination_stage     = VK_PIPELINE_STAGE_TRANSFER_BIT;

    } else if (
      old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
      new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    ) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      source_stage          = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destination_stage     = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else {
      logs::fatal("Could not complete requested layout transition as it's unsupported.");
    }

    vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage,
      0, 0, nullptr, 0, nullptr, 1, &barrier);

    end_command_buffer(device, queue, command_pool, command_buffer);
  }


  void copy_buffer_to_image(
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    VkBuffer buffer, VkImage image, u32 width, u32 height
  ) {
    VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);

    VkBufferImageCopy region = {
      .bufferOffset      = 0,
      .bufferRowLength   = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {
        .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel        = 0,
        .baseArrayLayer  = 0,
        .layerCount      = 1,
      },
      .imageOffset       = {0, 0, 0},
      .imageExtent       = {width, height, 1},
    };

    vkCmdCopyBufferToImage(command_buffer, buffer, image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    end_command_buffer(device, queue, command_pool, command_buffer);
  }


  VkShaderModule create_shader_module(
    VkDevice device, u8 const *shader, size_t size
  ) {
    VkShaderModuleCreateInfo const shader_module_info = {
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode    = (u32*)shader,
    };

    VkShaderModule shader_module;
    check(vkCreateShaderModule(device, &shader_module_info, nullptr,
      &shader_module));

    return shader_module;
  }


  VkShaderModule create_shader_module_from_file(
    VkDevice device, MemoryPool *pool, char const *path
  ) {
    size_t shader_size;
    u8 *shader = files::load_file_to_pool_u8(pool, path, &shader_size);
    return create_shader_module(device, shader, shader_size);
  }
}
