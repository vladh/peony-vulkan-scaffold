#include "intrinsics.hpp"
#include "vulkan.hpp"
#include "vkutils.hpp"
#include "vulkan_rendering.hpp"


namespace vulkan::forward_stage {
  static constexpr VkClearValue CLEAR_COLORS[] = {
    {{{0.0f, 0.0f, 0.0f, 1.0f}}},
    {{{1.0f, 0.0f}}},
  };

  static constexpr VkDescriptorSetLayoutBinding DESCRIPTOR_BINDINGS[] = {
    vkutils::descriptor_set_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
  };
  static constexpr u32 N_DESCRIPTORS = LEN(DESCRIPTOR_BINDINGS);


  static void render(VkState *vk_state, VkExtent2D extent, u32 idx_image) {
    auto idx_frame               = vk_state->idx_frame;
    auto *frame_resources        = &vk_state->frame_resources[idx_frame];
    auto *command_buffer         = &vk_state->forward_stage.command_buffers[idx_frame];
    auto global_descriptor_set   = vk_state->global_descriptor_sets[idx_frame];
    auto stage_descriptor_set    = vk_state->forward_stage.stage_descriptor_sets[idx_frame];
    auto material_descriptor_set = vk_state->material_descriptor_sets[idx_frame];
    auto entity_descriptor_set   = vk_state->entity_descriptor_sets[idx_frame];

    // Record command buffer
    {
      // Begin command buffer
      vkResetCommandBuffer(*command_buffer, 0);
      vkutils::begin_command_buffer(*command_buffer);

      // Begin render pass
      VkRenderPassBeginInfo const render_pass_info = vkutils::render_pass_begin_info(
        vk_state->forward_stage.render_pass,
        vk_state->forward_stage.framebuffers[idx_image],
        extent,
        LEN(forward_stage::CLEAR_COLORS),
        forward_stage::CLEAR_COLORS
      );
      vkCmdBeginRenderPass(*command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

      // Bind pipeline and descriptor sets
      VkDescriptorSet const descriptor_sets[] = {
        global_descriptor_set,
        stage_descriptor_set,
        material_descriptor_set,
        entity_descriptor_set,
      };
      vkCmdBindPipeline(*command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state->forward_stage.pipeline);
      vkCmdBindDescriptorSets(*command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_state->forward_stage.pipeline_layout,
        0, LEN(descriptor_sets), descriptor_sets, 0, nullptr);

      // Render
      range (0, vk_state->n_entities) {
        DrawableComponent *drawable_component = &vk_state->drawable_components[idx];
        if (has(drawable_component->target_render_stages, RenderStageName::forward_depth)) {
          rendering::render_drawable_component(drawable_component, command_buffer);
        }
      }

      // End render pass and command buffer
      vkCmdEndRenderPass(*command_buffer);
      vkutils::check(vkEndCommandBuffer(*command_buffer));
    }

    // Submit command buffer
    {
      VkSemaphore const wait_semaphores[] = {vk_state->lighting_stage.render_finished_semaphore};
      VkPipelineStageFlags const wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
      VkSemaphore const signal_semaphores[] = {vk_state->forward_stage.render_finished_semaphore};
      VkSubmitInfo const submit_info = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = wait_semaphores,
        .pWaitDstStageMask    = wait_stages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = signal_semaphores,
      };
      vkResetFences(vk_state->device, 1, &frame_resources->frame_rendered_fence);
      vkutils::check(vkQueueSubmit(vk_state->graphics_queue, 1, &submit_info, frame_resources->frame_rendered_fence));
    }
  }


  static void init_swapchain(VkState *vk_state, VkExtent2D extent) {
    // Command buffers
    {
      range (0, N_PARALLEL_FRAMES) {
        vkutils::create_command_buffer(vk_state->device, &vk_state->forward_stage.command_buffers[idx],
          vk_state->command_pool);
      }
    }

    // Descriptors
    {
      range (0, N_PARALLEL_FRAMES) {
        auto *stage_descriptor_set = &vk_state->forward_stage.stage_descriptor_sets[idx];

        // Allocate descriptor sets
        auto const alloc_info = vkutils::descriptor_set_allocate_info(vk_state->descriptor_pool,
          &vk_state->forward_stage.stage_descriptor_set_layout);
        vkutils::check(vkAllocateDescriptorSets(vk_state->device, &alloc_info, stage_descriptor_set));

        // Update descriptor sets
        VkDescriptorImageInfo const image_info = {
          .sampler     = vk_state->alpaca.sampler,
          .imageView   = vk_state->alpaca.view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkWriteDescriptorSet descriptor_writes[] = {
          vkutils::write_descriptor_set_image(*stage_descriptor_set, 0, &image_info),
        };
        vkUpdateDescriptorSets(vk_state->device, forward_stage::N_DESCRIPTORS, descriptor_writes, 0, nullptr);
      }
    }

    // Render pass
    {
      auto const color_attachment = vkutils::attachment_description_loadload(VK_FORMAT_B8G8R8A8_SRGB,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
      auto const color_attachment_ref = vkutils::attachment_reference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      auto const depthbuffer_attachment = vkutils::attachment_description_loadload(VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      auto const depthbuffer_attachment_ref = vkutils::attachment_reference(1,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

      VkAttachmentDescription const attachments[] = {color_attachment, depthbuffer_attachment};

      vkutils::create_render_pass(vk_state->device, &vk_state->forward_stage.render_pass,
        1, &color_attachment_ref,
        &depthbuffer_attachment_ref,
        LEN(attachments), attachments);
    }

    // Framebuffers
    {
      range (0, vk_state->n_swapchain_images) {
        VkImageView const attachments[] = {
          vk_state->swapchain_image_views[idx],
          vk_state->depthbuffer.view,
        };
        vkutils::create_framebuffer(vk_state->device, &vk_state->forward_stage.framebuffers[idx],
          vk_state->forward_stage.render_pass, LEN(attachments), attachments, extent);
      }
    }

    // Pipeline
    {
      // Pipeline layout
      VkDescriptorSetLayout const ds_layouts[] = {
        vk_state->global_descriptor_set_layout,
        vk_state->forward_stage.stage_descriptor_set_layout,
        vk_state->material_descriptor_set_layout,
        vk_state->entity_descriptor_set_layout,
      };
      auto const pipeline_layout_info = vkutils::pipeline_layout_create_info(LEN(ds_layouts), ds_layouts);
      vkutils::check(vkCreatePipelineLayout(vk_state->device, &pipeline_layout_info, nullptr,
        &vk_state->forward_stage.pipeline_layout));

      // Shaders
      MemoryPool pool = {};
      auto const vert_shader_module = vkutils::create_shader_module_from_file(vk_state->device, &pool,
        "bin/shaders/forward.vert.spv");
      auto const frag_shader_module = vkutils::create_shader_module_from_file(vk_state->device, &pool,
        "bin/shaders/forward.frag.spv");
      VkPipelineShaderStageCreateInfo const shader_stages[] = {
        vkutils::pipeline_shader_stage_create_info_vert(vert_shader_module),
        vkutils::pipeline_shader_stage_create_info_frag(frag_shader_module),
      };

      // Pipeline
      VkPipelineVertexInputStateCreateInfo const vertex_input_info = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &VERTEX_BINDING_DESCRIPTION,
        .vertexAttributeDescriptionCount = LEN(VERTEX_ATTRIBUTE_DESCRIPTIONS),
        .pVertexAttributeDescriptions    = VERTEX_ATTRIBUTE_DESCRIPTIONS,
      };
      VkPipelineInputAssemblyStateCreateInfo const input_assembly_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
      };
      VkViewport const viewport = vkutils::viewport_from_extent(extent);
      VkRect2D const scissor = vkutils::rect_from_extent(extent);
      auto const viewport_state_info = vkutils::pipeline_viewport_state_create_info(&viewport, &scissor);
      VkPipelineRasterizationStateCreateInfo const rasterizer_info = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
      };
      VkPipelineMultisampleStateCreateInfo const multisampling_info = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE,
      };
      VkPipelineDepthStencilStateCreateInfo const depth_stencil_info = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = VK_TRUE,
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
      };
      VkPipelineColorBlendAttachmentState color_blend_attachments[] = {
        vkutils::pipeline_color_blend_attachment_state(),
      };
      VkPipelineColorBlendStateCreateInfo const color_blending_info = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = LEN(color_blend_attachments),
        .pAttachments    = color_blend_attachments,
      };

      VkGraphicsPipelineCreateInfo const pipeline_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
        .layout              = vk_state->forward_stage.pipeline_layout,
        .renderPass          = vk_state->forward_stage.render_pass,
        .subpass             = 0,
      };

      vkutils::check(vkCreateGraphicsPipelines(vk_state->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
        &vk_state->forward_stage.pipeline));

      vkDestroyShaderModule(vk_state->device, vert_shader_module, nullptr);
      vkDestroyShaderModule(vk_state->device, frag_shader_module, nullptr);
    }
  }


  static void init(VkState *vk_state, VkExtent2D extent) {
    // Descriptor set layout
    {
      auto const layout_info = vkutils::descriptor_set_layout_create_info(forward_stage::N_DESCRIPTORS,
        forward_stage::DESCRIPTOR_BINDINGS);
      vkutils::check(vkCreateDescriptorSetLayout(vk_state->device, &layout_info, nullptr,
        &vk_state->forward_stage.stage_descriptor_set_layout));
    }

    vkutils::create_semaphore(vk_state->device, &vk_state->forward_stage.render_finished_semaphore);

    forward_stage::init_swapchain(vk_state, extent);
  }


  static void destroy_swapchain(VkState *vk_state) {
    range (0, N_PARALLEL_FRAMES) {
      vkFreeCommandBuffers(vk_state->device, vk_state->command_pool, 1, &vk_state->forward_stage.command_buffers[idx]);
    }
    range (0, vk_state->n_swapchain_images) {
      vkDestroyFramebuffer(vk_state->device, vk_state->forward_stage.framebuffers[idx], nullptr);
    }
    vkDestroyPipeline(vk_state->device, vk_state->forward_stage.pipeline, nullptr);
    vkDestroyPipelineLayout(vk_state->device, vk_state->forward_stage.pipeline_layout, nullptr);
    vkDestroyRenderPass(vk_state->device, vk_state->forward_stage.render_pass, nullptr);
  }


  static void destroy_nonswapchain(VkState *vk_state) {
    vkDestroyDescriptorSetLayout(vk_state->device, vk_state->forward_stage.stage_descriptor_set_layout, nullptr);
    vkDestroySemaphore(vk_state->device, vk_state->forward_stage.render_finished_semaphore, nullptr);
  }
}
