/*
  Functions that load all the resources we need into Vulkan objects.
*/


static void init_textures(VkState *vk_state) {
  // Load image
  int width, height, n_channels;
  unsigned char* image = files::load_image("resources/textures/alpaca.jpg",
    &width, &height, &n_channels, STBI_rgb_alpha, false);
  VkDeviceSize image_size = width * height * 4;

  // Copy to staging buffer
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  make_buffer(vk_state->device, vk_state->physical_device,
    image_size,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    &staging_buffer,
    &staging_buffer_memory);
  void *memory;
  vkMapMemory(vk_state->device, staging_buffer_memory, 0, image_size, 0, &memory);
  memcpy(memory, image, (size_t)image_size);
  vkUnmapMemory(vk_state->device, staging_buffer_memory);

  files::free_image(image);

  // Create VkImage
  make_image(vk_state->device, vk_state->physical_device,
    &vk_state->texture_image, &vk_state->texture_image_memory,
    width, height,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy image
  transition_image_layout(vk_state->device,
    vk_state->graphics_queue,
    vk_state->command_pool,
    vk_state->texture_image,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy_buffer_to_image(vk_state->device,
    vk_state->graphics_queue,
    vk_state->command_pool,
    staging_buffer,
    vk_state->texture_image,
    width,
    height);
  transition_image_layout(vk_state->device,
    vk_state->graphics_queue,
    vk_state->command_pool,
    vk_state->texture_image,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
  vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);

  // Create texture image view
  vk_state->texture_image_view = make_image_view(vk_state->device,
    vk_state->texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

  // Create sampler
  VkSamplerCreateInfo const sampler_info = {
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter               = VK_FILTER_LINEAR,
    .minFilter               = VK_FILTER_LINEAR,
    .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable        = VK_TRUE,
    .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .compareEnable           = VK_FALSE,
    .compareOp               = VK_COMPARE_OP_ALWAYS,
    .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .mipLodBias              = 0.0f,
    .minLod                  = 0.0f,
    .maxLod                  = 0.0f,
    .maxAnisotropy = vk_state->physical_device_properties.limits.maxSamplerAnisotropy,
  };
  if (
    vkCreateSampler(vk_state->device, &sampler_info, nullptr,
      &vk_state->texture_sampler) != VK_SUCCESS
  ) {
    logs::fatal("Could not create texture sampler.");
  }
}


static void init_buffers(VkState *vk_state) {
  // TODO: #slow Allocate memory only once, and split that up ourselves into the
  // two buffers using the memory offsets in e.g. `vkCmdBindVertexBuffers()`.
  // vulkan-tutorial.com/Vertex_buffers/Index_buffer.html

  // Vertex buffer
  {
    VkDeviceSize buffer_size = sizeof(VERTICES);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    make_buffer(vk_state->device, vk_state->physical_device,
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &staging_buffer,
      &staging_buffer_memory);

    void *memory;
    vkMapMemory(vk_state->device, staging_buffer_memory, 0, buffer_size, 0, &memory);
    memcpy(memory, VERTICES, (size_t)buffer_size);
    vkUnmapMemory(vk_state->device, staging_buffer_memory);

    make_buffer(vk_state->device, vk_state->physical_device,
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vk_state->vertex_buffer,
      &vk_state->vertex_buffer_memory);
    copy_buffer(vk_state->device, vk_state->command_pool, vk_state->graphics_queue,
      staging_buffer, vk_state->vertex_buffer, buffer_size);

    vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
    vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);
  }

  // Index buffer
  {
    VkDeviceSize buffer_size = sizeof(INDICES);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    make_buffer(vk_state->device, vk_state->physical_device,
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &staging_buffer,
      &staging_buffer_memory);

    void *memory;
    vkMapMemory(vk_state->device, staging_buffer_memory, 0, buffer_size, 0, &memory);
    memcpy(memory, INDICES, (size_t)buffer_size);
    vkUnmapMemory(vk_state->device, staging_buffer_memory);

    make_buffer(vk_state->device, vk_state->physical_device,
      buffer_size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vk_state->index_buffer,
      &vk_state->index_buffer_memory);
    copy_buffer(vk_state->device, vk_state->command_pool, vk_state->graphics_queue,
      staging_buffer, vk_state->index_buffer, buffer_size);

    vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
    vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);
  }
}
