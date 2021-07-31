/*
  Functions to assist with Vulkan tasks such as creating and copying buffers and
  images.

  These functions should not rely on VkState, but rather only on Vulkan types.
*/


static u32 find_memory_type(
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


static VkCommandBuffer begin_command_buffer(
  VkDevice device,
  VkCommandPool command_pool
) {
  VkCommandBufferAllocateInfo const alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandPool = command_pool,
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


static void end_command_buffer(
  VkDevice device,
  VkQueue queue,
  VkCommandPool command_pool,
  VkCommandBuffer command_buffer
) {
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo const submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &command_buffer,
  };
  vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

  vkQueueWaitIdle(queue);
  vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}


void make_buffer(
  VkDevice device, VkPhysicalDevice physical_device,
  VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
  VkBuffer *buffer, VkDeviceMemory *memory
) {
  VkBufferCreateInfo const buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  if (vkCreateBuffer(device, &buffer_info, nullptr, buffer) != VK_SUCCESS) {
    logs::fatal("Could not create buffer.");
  }

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device, *buffer, &requirements);

  u32 memory_type = find_memory_type(physical_device, requirements.memoryTypeBits,
    properties);
  VkMemoryAllocateInfo const alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = memory_type,
  };

  if (vkAllocateMemory(device, &alloc_info, nullptr, memory) != VK_SUCCESS) {
    logs::fatal("Could not allocate buffer memory.");
  }

  vkBindBufferMemory(device, *buffer, *memory, 0);
}


static void copy_buffer(
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


static void make_image(
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
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent.width = width,
    .extent.height = height,
    .extent.depth = 1,
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = format,
    .tiling = tiling,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .samples = VK_SAMPLE_COUNT_1_BIT,
  };
  if (vkCreateImage(device, &image_info, nullptr, image) != VK_SUCCESS) {
    logs::fatal("Could not create image.");
  }

  // Allocate memory
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device, *image, &requirements);
  VkMemoryAllocateInfo const alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = find_memory_type(physical_device,
      requirements.memoryTypeBits, properties),
  };
  if (
    vkAllocateMemory(device, &alloc_info, nullptr, image_memory) != VK_SUCCESS
  ) {
    logs::fatal("Could not allocate image memory.");
  }
  vkBindImageMemory(device, *image, *image_memory, 0);
}


static VkImageView make_image_view(
  VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags
) {
  VkImageViewCreateInfo const image_view_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .subresourceRange.aspectMask = aspect_flags,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
  };
  VkImageView image_view;
  if (
    vkCreateImageView(device, &image_view_info, nullptr,
      &image_view) != VK_SUCCESS
  ) {
    logs::fatal("Could not create texture image view.");
  }
  return image_view;
}


static void transition_image_layout(
  VkDevice device,
  VkQueue queue,
  VkCommandPool command_pool,
  VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout
) {
  VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);

  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = old_layout,
    .newLayout = new_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
    .srcAccessMask = 0, // Filled in later
    .dstAccessMask = 0, // Filled in later
  };

  VkPipelineStageFlags source_stage = {};
  VkPipelineStageFlags destination_stage = {};

  if (
    old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
    new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  ) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

  } else if (
    old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
    new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  ) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  } else {
    logs::fatal("Could not complete requested layout transition as it's unsupported.");
  }

  vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage,
    0, 0, nullptr, 0, nullptr, 1, &barrier);

  end_command_buffer(device, queue, command_pool, command_buffer);
}


static void copy_buffer_to_image(
  VkDevice device,
  VkQueue queue,
  VkCommandPool command_pool,
  VkBuffer buffer, VkImage image, u32 width, u32 height
) {
  VkCommandBuffer command_buffer = begin_command_buffer(device, command_pool);

  VkBufferImageCopy region = {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .imageSubresource.mipLevel = 0,
    .imageSubresource.baseArrayLayer = 0,
    .imageSubresource.layerCount = 1,
    .imageOffset = {0, 0, 0},
    .imageExtent = {width, height, 1},
  };

  vkCmdCopyBufferToImage(command_buffer, buffer, image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  end_command_buffer(device, queue, command_pool, command_buffer);
}
