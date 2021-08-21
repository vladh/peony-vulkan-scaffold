/*
  Functions that load all the resources we need into Vulkan objects.
*/


static void init_textures(VkState *vk_state) {
  // Load image
  int width, height, n_channels;
  unsigned char *image = files::load_image(
    "../peony/resources/textures/alpaca.jpg",
    &width, &height, &n_channels, STBI_rgb_alpha, false);
  VkDeviceSize image_size = width * height * 4;

  // Copy to staging buffer
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  vkutils::create_buffer(vk_state->device, vk_state->physical_device,
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
  vkutils::create_image(vk_state->device, vk_state->physical_device,
    &vk_state->texture_image, &vk_state->texture_image_memory,
    width, height,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy image
  vkutils::transition_image_layout(vk_state->device,
    vk_state->graphics_queue,
    vk_state->command_pool,
    vk_state->texture_image,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkutils::copy_buffer_to_image(vk_state->device,
    vk_state->graphics_queue,
    vk_state->command_pool,
    staging_buffer,
    vk_state->texture_image,
    width,
    height);
  vkutils::transition_image_layout(vk_state->device,
    vk_state->graphics_queue,
    vk_state->command_pool,
    vk_state->texture_image,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
  vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);

  // Create texture image view
  vk_state->texture_image_view = vkutils::create_image_view(vk_state->device,
    vk_state->texture_image, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_ASPECT_COLOR_BIT);

  // Create sampler
  VkSamplerCreateInfo const sampler_info = vkutils::sampler_create_info(
    vk_state->physical_device_properties);
  vkutils::check(vkCreateSampler(vk_state->device, &sampler_info, nullptr,
    &vk_state->texture_sampler));
}


static void init_uniform_buffers(VkState *vk_state) {
  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      sizeof(CoreSceneState),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &frame_resources->uniform_buffer,
      &frame_resources->uniform_buffer_memory);
  }
}


static void init_buffers(VkState *vk_state) {
  // TODO: #slow Allocate memory only once, and split that up ourselves into the
  // two buffers using the memory offsets in e.g. `vkCmdBindVertexBuffers()`.
  // vulkan-tutorial.com/Vertex_buffers/Index_buffer.html

  // Sign vertex buffer
  {
    VkDeviceSize buffer_size = sizeof(SIGN_VERTICES);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
      &staging_buffer_memory);

    void *memory;
    vkMapMemory(vk_state->device, staging_buffer_memory, 0, buffer_size, 0,
      &memory);
    memcpy(memory, SIGN_VERTICES, (size_t)buffer_size);
    vkUnmapMemory(vk_state->device, staging_buffer_memory);

    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vk_state->sign_vertex_buffer, &vk_state->sign_vertex_buffer_memory);
    vkutils::copy_buffer(vk_state->device, vk_state->command_pool,
      vk_state->graphics_queue, staging_buffer,
      vk_state->sign_vertex_buffer, buffer_size);

    vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
    vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);
  }

  // Sign index buffer
  {
    VkDeviceSize buffer_size = sizeof(SIGN_INDICES);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
      &staging_buffer_memory);

    void *memory;
    vkMapMemory(vk_state->device, staging_buffer_memory, 0, buffer_size, 0,
      &memory);
    memcpy(memory, SIGN_INDICES, (size_t)buffer_size);
    vkUnmapMemory(vk_state->device, staging_buffer_memory);

    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vk_state->sign_index_buffer, &vk_state->sign_index_buffer_memory);
    vkutils::copy_buffer(vk_state->device, vk_state->command_pool,
      vk_state->graphics_queue, staging_buffer, vk_state->sign_index_buffer,
      buffer_size);

    vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
    vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);
  }

  // Screenquad vertex buffer
  {
    VkDeviceSize buffer_size = sizeof(SCREENQUAD_VERTICES);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
      &staging_buffer_memory);

    void *memory;
    vkMapMemory(vk_state->device, staging_buffer_memory, 0, buffer_size, 0,
      &memory);
    memcpy(memory, SCREENQUAD_VERTICES, (size_t)buffer_size);
    vkUnmapMemory(vk_state->device, staging_buffer_memory);

    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vk_state->screenquad_vertex_buffer,
      &vk_state->screenquad_vertex_buffer_memory);
    vkutils::copy_buffer(vk_state->device, vk_state->command_pool,
      vk_state->graphics_queue, staging_buffer,
      vk_state->screenquad_vertex_buffer, buffer_size);

    vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
    vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);
  }

  // Screenquad index buffer
  {
    VkDeviceSize buffer_size = sizeof(SCREENQUAD_INDICES);

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer,
      &staging_buffer_memory);

    void *memory;
    vkMapMemory(vk_state->device, staging_buffer_memory, 0, buffer_size, 0,
      &memory);
    memcpy(memory, SCREENQUAD_INDICES, (size_t)buffer_size);
    vkUnmapMemory(vk_state->device, staging_buffer_memory);

    vkutils::create_buffer(vk_state->device, vk_state->physical_device,
      buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vk_state->screenquad_index_buffer,
      &vk_state->screenquad_index_buffer_memory);
    vkutils::copy_buffer(vk_state->device, vk_state->command_pool,
      vk_state->graphics_queue, staging_buffer,
      vk_state->screenquad_index_buffer, buffer_size);

    vkDestroyBuffer(vk_state->device, staging_buffer, nullptr);
    vkFreeMemory(vk_state->device, staging_buffer_memory, nullptr);
  }
}
