static void init_deferred_synchronization(VkState *vk_state) {
  VkSemaphoreCreateInfo const semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  check_vk_result(vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
    &vk_state->deferred_stage.render_finished_semaphore));
}


static void init_deferred_command_buffer(VkState *vk_state, VkExtent2D extent) {
  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];

    VkCommandBufferAllocateInfo const alloc_info =
      command_buffer_allocate_info(vk_state->command_pool);
    check_vk_result(vkAllocateCommandBuffers(vk_state->device, &alloc_info,
      &frame_resources->deferred_command_buffer));
  }
}


static void init_deferred_descriptor_set_layout(VkState *vk_state) {
  u32 n_descriptors = 2;

  // Create descriptor set layout
  VkDescriptorSetLayoutBinding bindings[] = {
    descriptor_set_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
    descriptor_set_layout_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
  };
  VkDescriptorSetLayoutCreateInfo const layout_info =
    descriptor_set_layout_create_info(n_descriptors, bindings);
  check_vk_result(vkCreateDescriptorSetLayout(vk_state->device, &layout_info,
    nullptr, &vk_state->deferred_stage.descriptor_set_layout));
}


static void init_deferred_descriptors(VkState *vk_state) {
  u32 n_descriptors = 2;

  // Create descriptor pool
  VkDescriptorPoolSize pool_sizes[] = {
    descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      N_PARALLEL_FRAMES),
    descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      N_PARALLEL_FRAMES),
  };
  VkDescriptorPoolCreateInfo const pool_info = descriptor_pool_create_info(
    N_PARALLEL_FRAMES, n_descriptors, pool_sizes);
  check_vk_result(vkCreateDescriptorPool(vk_state->device, &pool_info, nullptr,
    &vk_state->deferred_stage.descriptor_pool));

  // Image info is always the same
  VkDescriptorImageInfo const image_info = {
    .sampler     = vk_state->texture_sampler,
    .imageView   = vk_state->texture_image_view,
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  // Create descriptors
  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];

    VkDescriptorBufferInfo const buffer_info = {
      .buffer = frame_resources->uniform_buffer,
      .offset = 0,
      .range  = sizeof(CoreSceneState),
    };

    // Create descriptor sets
    VkDescriptorSetAllocateInfo const alloc_info = descriptor_set_allocate_info(
      vk_state->deferred_stage.descriptor_pool,
      &vk_state->deferred_stage.descriptor_set_layout);
    check_vk_result(vkAllocateDescriptorSets(vk_state->device, &alloc_info,
      &frame_resources->deferred_descriptor_set));

    // Update descriptor sets
    VkWriteDescriptorSet descriptor_writes[] = {
      write_descriptor_set_buffer(frame_resources->deferred_descriptor_set, 0,
        &buffer_info),
      write_descriptor_set_image(frame_resources->deferred_descriptor_set, 1,
        &image_info),
    };
    vkUpdateDescriptorSets(vk_state->device, n_descriptors, descriptor_writes,
      0, nullptr);
  }
}


static void init_deferred_render_pass(VkState *vk_state) {
  VkAttachmentDescription const g_position_attachment = attachment_description(
    VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkAttachmentReference const g_position_ref = attachment_reference(
    0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkAttachmentDescription const g_normal_attachment = attachment_description(
    VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkAttachmentReference const g_normal_ref = attachment_reference(
    1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkAttachmentDescription const g_albedo_attachment = attachment_description(
    VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkAttachmentReference const g_albedo_ref = attachment_reference(
    2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkAttachmentDescription const g_pbr_attachment = attachment_description(
    VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkAttachmentReference const g_pbr_ref = attachment_reference(
    3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkAttachmentDescription const depthbuffer_attachment = attachment_description(
    VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  VkAttachmentReference const depthbuffer_attachment_ref = attachment_reference(
    4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  VkAttachmentReference const color_attachment_refs[] = {
    g_position_ref, g_normal_ref, g_albedo_ref, g_pbr_ref
  };

  VkAttachmentDescription const attachments[] = {
    g_position_attachment, g_normal_attachment, g_albedo_attachment,
    g_pbr_attachment, depthbuffer_attachment
  };

  VkSubpassDescription const subpass = {
    .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount    = LEN(color_attachment_refs),
    .pColorAttachments       = color_attachment_refs,
    .pDepthStencilAttachment = &depthbuffer_attachment_ref,
  };
  VkSubpassDependency const dependency = {
    .srcSubpass    = VK_SUBPASS_EXTERNAL,
    .dstSubpass    = 0,
    .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo const render_pass_info = {
    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = LEN(attachments),
    .pAttachments    = attachments,
    .subpassCount    = 1,
    .pSubpasses      = &subpass,
    .dependencyCount = 1,
    .pDependencies   = &dependency,
  };

  check_vk_result(vkCreateRenderPass(vk_state->device, &render_pass_info,
    nullptr, &vk_state->deferred_stage.render_pass));
}


static void init_deferred_pipeline(VkState *vk_state, VkExtent2D extent) {
  // Pipeline layout
  VkPipelineLayoutCreateInfo const pipeline_layout_info =
    pipeline_layout_create_info(&vk_state->deferred_stage.descriptor_set_layout);
  check_vk_result(vkCreatePipelineLayout(vk_state->device, &pipeline_layout_info,
    nullptr, &vk_state->deferred_stage.pipeline_layout));

  // Shaders
  MemoryPool pool = {};
  VkShaderModule const vert_shader_module = create_shader_module_from_file(
    vk_state->device, &pool, "bin/shaders/standard.vert.spv");
  VkShaderModule const frag_shader_module = create_shader_module_from_file(
    vk_state->device, &pool, "bin/shaders/standard.frag.spv");
  VkPipelineShaderStageCreateInfo const shader_stages[] = {
    pipeline_shader_stage_create_info_vert(vert_shader_module),
    pipeline_shader_stage_create_info_frag(frag_shader_module),
  };

  // Pipeline
  VkPipelineVertexInputStateCreateInfo const vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount   = 1,
    .pVertexBindingDescriptions      = &VERTEX_BINDING_DESCRIPTION,
    .vertexAttributeDescriptionCount = LEN(VERTEX_ATTRIBUTE_DESCRIPTIONS),
    .pVertexAttributeDescriptions    = VERTEX_ATTRIBUTE_DESCRIPTIONS,
  };
  VkPipelineInputAssemblyStateCreateInfo const input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };
  VkViewport const viewport = {
    .x        = 0.0f,
    .y        = 0.0f,
    .width    = (f32)extent.width,
    .height   = (f32)extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };
  VkRect2D const scissor = {
    .offset = {0, 0},
    .extent = extent,
  };
  VkPipelineViewportStateCreateInfo const viewport_state_info =
    pipeline_viewport_state_create_info(&viewport, &scissor);
  VkPipelineRasterizationStateCreateInfo const rasterizer_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable        = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode             = VK_POLYGON_MODE_FILL,
    .cullMode                = VK_CULL_MODE_BACK_BIT,
    .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable         = VK_FALSE,
    .lineWidth               = 1.0f,
  };
  VkPipelineMultisampleStateCreateInfo const multisampling_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable  = VK_FALSE,
  };
  VkPipelineDepthStencilStateCreateInfo const depth_stencil_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable       = VK_TRUE,
    .depthWriteEnable      = VK_TRUE,
    .depthCompareOp        = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable     = VK_FALSE,
  };
  VkPipelineColorBlendAttachmentState color_blend_attachments[] = {
    pipeline_color_blend_attachment_state(),
    pipeline_color_blend_attachment_state(),
    pipeline_color_blend_attachment_state(),
    pipeline_color_blend_attachment_state(),
  };
  VkPipelineColorBlendStateCreateInfo const color_blending_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable   = VK_FALSE,
    .attachmentCount = LEN(color_blend_attachments),
    .pAttachments    = color_blend_attachments,
  };

  VkGraphicsPipelineCreateInfo const pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount          = 2,
    .pStages             = shader_stages,
    .pVertexInputState   = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState      = &viewport_state_info,
    .pRasterizationState = &rasterizer_info,
    .pMultisampleState   = &multisampling_info,
    .pDepthStencilState  = &depth_stencil_info,
    .pColorBlendState    = &color_blending_info,
    .pDynamicState       = nullptr,
    .layout              = vk_state->deferred_stage.pipeline_layout,
    .renderPass          = vk_state->deferred_stage.render_pass,
    .subpass             = 0,
  };

  check_vk_result(vkCreateGraphicsPipelines(vk_state->device, VK_NULL_HANDLE, 1,
    &pipeline_info, nullptr, &vk_state->deferred_stage.pipeline));

  vkDestroyShaderModule(vk_state->device, vert_shader_module, nullptr);
  vkDestroyShaderModule(vk_state->device, frag_shader_module, nullptr);
}


static void init_deferred_framebuffers(VkState *vk_state, VkExtent2D extent) {
  // g_position
  create_image_resources(&vk_state->g_position,
    vk_state->device, vk_state->physical_device,
    extent.width, extent.height,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT);
  VkSamplerCreateInfo const sampler_info = sampler_create_info(
    vk_state->physical_device_properties);
  check_vk_result(vkCreateSampler(vk_state->device, &sampler_info, nullptr,
    &vk_state->g_position.sampler));

  // g_normal
  create_image_resources(&vk_state->g_normal,
    vk_state->device, vk_state->physical_device,
    extent.width, extent.height,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT);

  // g_albedo
  create_image_resources(&vk_state->g_albedo,
    vk_state->device, vk_state->physical_device,
    extent.width, extent.height,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT);

  // g_pbr
  create_image_resources(&vk_state->g_pbr,
    vk_state->device, vk_state->physical_device,
    extent.width, extent.height,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT);

  // Depth buffer
  create_image_resources(&vk_state->depthbuffer,
    vk_state->device, vk_state->physical_device,
    extent.width, extent.height,
    VK_FORMAT_D32_SFLOAT,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    VK_IMAGE_ASPECT_DEPTH_BIT);

  // Framebuffers
  range (0, vk_state->n_swapchain_images) {
    VkImageView const attachments[] = {
      /* vk_state->swapchain_image_views[idx], */
      vk_state->g_position.view,
      vk_state->g_normal.view,
      vk_state->g_albedo.view,
      vk_state->g_pbr.view,
      vk_state->depthbuffer.view,
    };
    VkFramebufferCreateInfo const framebuffer_info = {
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = vk_state->deferred_stage.render_pass,
      .attachmentCount = LEN(attachments),
      .pAttachments    = attachments,
      .width           = extent.width,
      .height          = extent.height,
      .layers          = 1,
    };
    check_vk_result(vkCreateFramebuffer(vk_state->device, &framebuffer_info,
      nullptr, &vk_state->deferred_stage.framebuffers[idx]));
  }
}


static void record_deferred_command_buffer(
  VkState *vk_state,
  VkCommandBuffer *command_buffer,
  VkExtent2D extent,
  u32 idx_frame,
  u32 idx_image
) {
  FrameResources *frame_resources = &vk_state->frame_resources[idx_frame];
  VkDescriptorSet *descriptor_set = &frame_resources->deferred_descriptor_set;

  // Reset commmand buffer
  vkResetCommandBuffer(*command_buffer, 0);

  // Begin command buffer
  VkCommandBufferBeginInfo const buffer_info = command_buffer_begin_info();
  check_vk_result(vkBeginCommandBuffer(*command_buffer, &buffer_info));

  // Begin render pass
  VkClearValue const clear_colors[] = {
    {{{0.0f, 0.0f, 0.0f, 1.0f}}},
    {{{0.0f, 0.0f, 0.0f, 1.0f}}},
    {{{0.0f, 0.0f, 0.0f, 1.0f}}},
    {{{0.0f, 0.0f, 0.0f, 1.0f}}},
    {{{1.0f, 0.0f}}},
  };
  VkRenderPassBeginInfo const renderpass_info = {
    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass      = vk_state->deferred_stage.render_pass,
    .framebuffer     = vk_state->deferred_stage.framebuffers[idx_image],
    .renderArea = {
      .offset        = {0, 0},
      .extent        = extent,
    },
    .clearValueCount = LEN(clear_colors),
    .pClearValues    = clear_colors,
  };
  vkCmdBeginRenderPass(*command_buffer, &renderpass_info,
    VK_SUBPASS_CONTENTS_INLINE);

  // Bind pipeline and descriptor sets
  vkCmdBindPipeline(*command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    vk_state->deferred_stage.pipeline);
  vkCmdBindDescriptorSets(*command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    vk_state->deferred_stage.pipeline_layout, 0, 1, descriptor_set, 0, nullptr);

  // Bind vertex and index buffers
  VkBuffer const vertex_buffers[] = {vk_state->vertex_buffer};
  VkDeviceSize const offsets[] = {0};
  vkCmdBindVertexBuffers(*command_buffer, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(*command_buffer, vk_state->index_buffer, 0,
    VK_INDEX_TYPE_UINT32);

  // Draw
  vkCmdDrawIndexed(*command_buffer, LEN(INDICES), 1, 0, 0, 0);

  // End render pass and command buffer
  vkCmdEndRenderPass(*command_buffer);
  check_vk_result(vkEndCommandBuffer(*command_buffer));
}
