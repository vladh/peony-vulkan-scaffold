/*
  Functions that load all the resources we need into Vulkan objects.
*/


static void init_textures(VkState *vk_state) {
  // Load alpaca
  {
    int width, height, n_channels;
    unsigned char *image = files::load_image("../peony/resources/textures/alpaca.jpg", &width, &height, &n_channels,
      STBI_rgb_alpha, false);
    defer { files::free_image(image); };

    vkutils::create_image_resources_with_sampler_from_image(
      vk_state->device,
      &vk_state->alpaca,
      vk_state->physical_device,
      image,
      width, height,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      VK_IMAGE_ASPECT_COLOR_BIT,
      vk_state->physical_device_properties,
      vk_state->graphics_queue,
      vk_state->command_pool);
  }
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

  // Sign
  vkutils::create_buffer_resources(vk_state->device,
    &vk_state->sign.vertex,
    vk_state->physical_device,
    SIGN_VERTICES,
    LEN(SIGN_VERTICES),
    sizeof(SIGN_VERTICES),
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    vk_state->command_pool,
    vk_state->graphics_queue);
  vkutils::create_buffer_resources(vk_state->device,
    &vk_state->sign.index,
    vk_state->physical_device,
    SIGN_INDICES,
    LEN(SIGN_INDICES),
    sizeof(SIGN_INDICES),
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    vk_state->command_pool,
    vk_state->graphics_queue);

  // Fsign
  vkutils::create_buffer_resources(vk_state->device,
    &vk_state->fsign.vertex,
    vk_state->physical_device,
    FSIGN_VERTICES,
    LEN(FSIGN_VERTICES),
    sizeof(FSIGN_VERTICES),
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    vk_state->command_pool,
    vk_state->graphics_queue);
  vkutils::create_buffer_resources(vk_state->device,
    &vk_state->fsign.index,
    vk_state->physical_device,
    FSIGN_INDICES,
    LEN(FSIGN_INDICES),
    sizeof(FSIGN_INDICES),
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    vk_state->command_pool,
    vk_state->graphics_queue);

  // Screenquad
  vkutils::create_buffer_resources(vk_state->device,
    &vk_state->screenquad.vertex,
    vk_state->physical_device,
    SCREENQUAD_VERTICES,
    LEN(SCREENQUAD_VERTICES),
    sizeof(SCREENQUAD_VERTICES),
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    vk_state->command_pool,
    vk_state->graphics_queue);
  vkutils::create_buffer_resources(vk_state->device,
    &vk_state->screenquad.index,
    vk_state->physical_device,
    SCREENQUAD_INDICES,
    LEN(SCREENQUAD_INDICES),
    sizeof(SCREENQUAD_INDICES),
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    vk_state->command_pool,
    vk_state->graphics_queue);
}
