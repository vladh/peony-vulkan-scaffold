#include <stdint.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "glm.hpp"
#include "common.hpp"
#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "files.hpp"

#include "vulkan_utils.cpp"
#include "vulkan_swapchain.cpp"
#include "vulkan_resources.cpp"
#include "vulkan_general.cpp"


static void init_descriptors(VkState *vk_state) {
  // Create descriptor set layout
  u32 n_descriptors = 2;
  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .binding         = 0,
      .descriptorCount = 1,
      .stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS,
      .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    },
    {
      .binding         = 1,
      .descriptorCount = 1,
      .stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS,
      .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    }
  };
  VkDescriptorSetLayoutCreateInfo const layout_info = {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = n_descriptors,
    .pBindings    = bindings,
  };
  if (
    vkCreateDescriptorSetLayout(vk_state->device, &layout_info, nullptr,
      &vk_state->descriptor_set_layout) != VK_SUCCESS
  ) {
    logs::fatal("Could not create descriptor set layout.");
  }

  // Create descriptor pool
  VkDescriptorPoolSize pool_sizes[] = {
    {
      .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = vk_state->n_swapchain_images,
    },
    {
      .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = vk_state->n_swapchain_images,
    }
  };
  VkDescriptorPoolCreateInfo const pool_info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = n_descriptors,
    .pPoolSizes    = pool_sizes,
    .maxSets       = vk_state->n_swapchain_images,
  };
  if (
    vkCreateDescriptorPool(vk_state->device, &pool_info, nullptr,
      &vk_state->descriptor_pool) != VK_SUCCESS
  ) {
    logs::fatal("Could not create descriptor pool.");
  }

  // Image info is always the same
  VkDescriptorImageInfo const image_info = {
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .imageView   = vk_state->texture_image_view,
    .sampler     = vk_state->texture_sampler,
  };

  // Create uniform buffers and descriptors
  range (0, vk_state->n_swapchain_images) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];

    // Create uniform buffer
    make_buffer(vk_state->device, vk_state->physical_device,
      sizeof(CoreSceneState),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &frame_resources->uniform_buffer,
      &frame_resources->uniform_buffer_memory);
    VkDescriptorBufferInfo const buffer_info = {
      .buffer = frame_resources->uniform_buffer,
      .offset = 0,
      .range  = sizeof(CoreSceneState),
    };

    // Create descriptor sets
    VkDescriptorSetAllocateInfo const alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool     = vk_state->descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts        = &vk_state->descriptor_set_layout,
    };
    if (vkAllocateDescriptorSets(vk_state->device, &alloc_info,
        &frame_resources->descriptor_set) != VK_SUCCESS) {
      logs::fatal("Could not allocate descriptor sets.");
    }

    // Update descriptor sets
    VkWriteDescriptorSet descriptor_writes[] = {
      {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = frame_resources->descriptor_set,
        .dstBinding      = 0,
        .dstArrayElement = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo     = &buffer_info,
      },
      {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = frame_resources->descriptor_set,
        .dstBinding      = 1,
        .dstArrayElement = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo      = &image_info,
      },
    };
    vkUpdateDescriptorSets(vk_state->device, n_descriptors, descriptor_writes, 0,
      nullptr);
  }
}


static void init_render_pass(VkState *vk_state) {
  VkAttachmentDescription const color_attachment = {
    .format         = vk_state->swapchain_image_format,
    .samples        = VK_SAMPLE_COUNT_1_BIT,
    .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };
  VkAttachmentReference const color_attachment_ref = {
    .attachment = 0,
    .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentDescription const depth_attachment = {
    .format         = VK_FORMAT_D32_SFLOAT,
    .samples        = VK_SAMPLE_COUNT_1_BIT,
    .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  VkAttachmentReference const depth_attachment_ref = {
    .attachment = 1,
    .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription const subpass = {
    .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount    = 1,
    .pColorAttachments       = &color_attachment_ref,
    .pDepthStencilAttachment = &depth_attachment_ref,
  };
  VkSubpassDependency const dependency = {
    .srcSubpass    = VK_SUBPASS_EXTERNAL,
    .dstSubpass    = 0,
    .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .srcAccessMask = 0,
    .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
  };

  VkAttachmentDescription const attachments[] = {color_attachment, depth_attachment};
  VkRenderPassCreateInfo const render_pass_info = {
    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = LEN(attachments),
    .pAttachments    = attachments,
    .subpassCount    = 1,
    .pSubpasses      = &subpass,
    .dependencyCount = 1,
    .pDependencies   = &dependency,
  };

  if (
    vkCreateRenderPass(vk_state->device, &render_pass_info, nullptr,
      &vk_state->render_pass) != VK_SUCCESS
  ) {
    logs::fatal("Could not create render pass.");
  }
}


static void init_pipeline(VkState *vk_state, VkExtent2D extent) {
  // Pipeline layout
  VkPipelineLayoutCreateInfo const pipeline_layout_info = {
    .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts    = &vk_state->descriptor_set_layout,
  };
  if (
    vkCreatePipelineLayout(vk_state->device, &pipeline_layout_info, nullptr,
      &vk_state->pipeline_layout) != VK_SUCCESS
  ) {
    logs::fatal("Could not create pipeline layout.");
  }

  // Shaders
  MemoryPool pool = {};

  size_t vert_shader_size;
  u8 *vert_shader = files::load_file_to_pool_u8(&pool,
    "bin/shaders/test.vert.spv", &vert_shader_size);
  VkShaderModule const vert_shader_module = make_shader_module(vk_state->device,
    vert_shader, vert_shader_size);
  VkPipelineShaderStageCreateInfo const vert_shader_stage_info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage  = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vert_shader_module,
    .pName  = "main",
  };

  size_t frag_shader_size;
  u8 *frag_shader = files::load_file_to_pool_u8(&pool,
    "bin/shaders/test.frag.spv", &frag_shader_size);
  VkShaderModule const frag_shader_module = make_shader_module(vk_state->device,
    frag_shader, frag_shader_size);
  VkPipelineShaderStageCreateInfo const frag_shader_stage_info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = frag_shader_module,
    .pName  = "main",
  };

  VkPipelineShaderStageCreateInfo const shader_stages[] = {
    vert_shader_stage_info,
    frag_shader_stage_info,
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
  VkPipelineViewportStateCreateInfo const viewport_state_info = {
    .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports    = &viewport,
    .scissorCount  = 1,
    .pScissors     = &scissor,
  };
  VkPipelineRasterizationStateCreateInfo const rasterizer_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable        = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode             = VK_POLYGON_MODE_FILL,
    .lineWidth               = 1.0f,
    .cullMode                = VK_CULL_MODE_BACK_BIT,
    .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable         = VK_FALSE,
  };
  VkPipelineMultisampleStateCreateInfo const multisampling_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable  = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };
  VkPipelineDepthStencilStateCreateInfo const depth_stencil_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable       = VK_TRUE,
    .depthWriteEnable      = VK_TRUE,
    .depthCompareOp        = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable     = VK_FALSE,
  };
  VkPipelineColorBlendAttachmentState color_blend_attachment = {
    .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable         = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp        = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp        = VK_BLEND_OP_ADD,
  };
  VkPipelineColorBlendStateCreateInfo const color_blending_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable   = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments    = &color_blend_attachment,
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
    .layout              = vk_state->pipeline_layout,
    .renderPass          = vk_state->render_pass,
    .subpass             = 0,
  };

  if (
    vkCreateGraphicsPipelines(vk_state->device, VK_NULL_HANDLE, 1, &pipeline_info,
      nullptr, &vk_state->pipeline) != VK_SUCCESS
  ) {
    logs::fatal("Could not create graphics pipeline.");
  }

  vkDestroyShaderModule(vk_state->device, vert_shader_module, nullptr);
  vkDestroyShaderModule(vk_state->device, frag_shader_module, nullptr);
}


static void init_framebuffers(VkState *vk_state, VkExtent2D extent) {
  // Depth buffer
  make_image(vk_state->device, vk_state->physical_device,
    &vk_state->depth_image, &vk_state->depth_image_memory,
    extent.width, extent.height,
    VK_FORMAT_D32_SFLOAT,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vk_state->depth_image_view = make_image_view(vk_state->device,
    vk_state->depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

  // Framebuffers
  range (0, vk_state->n_swapchain_images) {
    VkImageView const attachments[] = {
      vk_state->swapchain_image_views[idx],
      vk_state->depth_image_view,
    };
    VkFramebufferCreateInfo const framebuffer_info = {
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = vk_state->render_pass,
      .attachmentCount = LEN(attachments),
      .pAttachments    = attachments,
      .width           = extent.width,
      .height          = extent.height,
      .layers          = 1,
    };
    if (
      vkCreateFramebuffer(vk_state->device, &framebuffer_info, nullptr,
        &vk_state->swapchain_framebuffers[idx]) != VK_SUCCESS
    ) {
      logs::fatal("Could not create framebuffer.");
    }
  }
}


static void init_command_buffers(VkState *vk_state, VkExtent2D extent) {
  range (0, vk_state->n_swapchain_images) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    VkCommandBufferAllocateInfo const alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool        = vk_state->command_pool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    };

    if (
      vkAllocateCommandBuffers(vk_state->device, &alloc_info,
        &vk_state->frame_resources[idx].command_buffer)
      != VK_SUCCESS
    ) {
      logs::fatal("Could not allocate command buffers.");
    }

    VkCommandBuffer const command_buffer = vk_state->frame_resources[idx].command_buffer;

    VkCommandBufferBeginInfo const buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if (vkBeginCommandBuffer(command_buffer, &buffer_info) != VK_SUCCESS) {
      logs::fatal("Could not begin recording command buffer.");
    }

    VkClearValue const clear_colors[] = {
      {{{0.0f, 0.0f, 0.0f, 1.0f}}},
      {{{1.0f, 0.0f}}},
    };
    VkRenderPassBeginInfo const renderpass_info = {
      .sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass        = vk_state->render_pass,
      .framebuffer       = vk_state->swapchain_framebuffers[idx],
      .renderArea.offset = {0, 0},
      .renderArea.extent = extent,
      .clearValueCount   = LEN(clear_colors),
      .pClearValues      = clear_colors,
    };

    vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk_state->pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk_state->pipeline_layout, 0, 1, &frame_resources->descriptor_set, 0, nullptr);

    {
      VkBuffer const vertex_buffers[] = {vk_state->vertex_buffer};
      VkDeviceSize const offsets[] = {0};
      vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
      vkCmdBindIndexBuffer(command_buffer, vk_state->index_buffer, 0,
        VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(command_buffer, LEN(INDICES), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
      logs::fatal("Could not record command buffer.");
    }
  }
}


static void init_frame_resources(VkState *vk_state) {
  // Command buffer is initialised separately
  VkSemaphoreCreateInfo const semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  range (0, vk_state->n_swapchain_images) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    if (
      vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
        &frame_resources->image_available_semaphore) != VK_SUCCESS ||
      vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
        &frame_resources->render_finished_semaphore) != VK_SUCCESS ||
      vkCreateFence(vk_state->device, &fence_info, nullptr,
        &frame_resources->frame_rendered_fence) != VK_SUCCESS
    ) {
      logs::fatal("Could not create semaphores.");
    }
  }
}


void vulkan::init(VkState *vk_state, CommonState *common_state) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};

  if (USE_VALIDATION) {
    if (!ensure_validation_layers_supported()) {
      logs::fatal("Could not get required validation layers.");
    }

    debug_messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity =
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | */
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = debug_callback;
  }


  init_instance(vk_state, &debug_messenger_info);
  init_debug_messenger(vk_state, &debug_messenger_info);
  init_surface(vk_state, common_state->window);
  init_physical_device(vk_state);
  init_logical_device(vk_state);
  init_swapchain(vk_state, common_state->window, &common_state->extent);
  init_render_pass(vk_state);
  init_framebuffers(vk_state, common_state->extent);
  init_command_pool(vk_state);
  init_textures(vk_state);
  init_descriptors(vk_state);
  init_buffers(vk_state);
  init_pipeline(vk_state, common_state->extent);
  init_command_buffers(vk_state, common_state->extent);
  init_frame_resources(vk_state);
}


static void destroy_swapchain(VkState *vk_state) {
  vkDestroyImageView(vk_state->device, vk_state->depth_image_view, nullptr);
  vkDestroyImage(vk_state->device, vk_state->depth_image, nullptr);
  vkFreeMemory(vk_state->device, vk_state->depth_image_memory, nullptr);

  range (0, vk_state->n_swapchain_images) {
    vkDestroyFramebuffer(vk_state->device, vk_state->swapchain_framebuffers[idx],
      nullptr);
    vkFreeCommandBuffers(vk_state->device, vk_state->command_pool,
      1, &vk_state->frame_resources[idx].command_buffer);
  }
  vkDestroyPipeline(vk_state->device, vk_state->pipeline, nullptr);
  vkDestroyPipelineLayout(vk_state->device, vk_state->pipeline_layout, nullptr);
  vkDestroyRenderPass(vk_state->device, vk_state->render_pass, nullptr);
  range (0, vk_state->n_swapchain_images) {
    vkDestroyImageView(vk_state->device, vk_state->swapchain_image_views[idx], nullptr);
  }
  vkDestroySwapchainKHR(vk_state->device, vk_state->swapchain, nullptr);
}


void vulkan::destroy(VkState *vk_state) {
  destroy_swapchain(vk_state);

  vkDestroySampler(vk_state->device, vk_state->texture_sampler, nullptr);
  vkDestroyImageView(vk_state->device, vk_state->texture_image_view, nullptr);
  vkDestroyImage(vk_state->device, vk_state->texture_image, nullptr);
  vkFreeMemory(vk_state->device, vk_state->texture_image_memory, nullptr);

  vkDestroyDescriptorSetLayout(vk_state->device,
    vk_state->descriptor_set_layout, nullptr);

  vkDestroyBuffer(vk_state->device, vk_state->vertex_buffer, nullptr);
  vkFreeMemory(vk_state->device, vk_state->vertex_buffer_memory, nullptr);
  vkDestroyBuffer(vk_state->device, vk_state->index_buffer, nullptr);
  vkFreeMemory(vk_state->device, vk_state->index_buffer_memory, nullptr);

  vkDestroyDescriptorPool(vk_state->device, vk_state->descriptor_pool, nullptr);

  range (0, vk_state->n_swapchain_images) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];

    vkDestroyBuffer(vk_state->device, frame_resources->uniform_buffer, nullptr);
    vkFreeMemory(vk_state->device, frame_resources->uniform_buffer_memory, nullptr);

    vkDestroySemaphore(vk_state->device, frame_resources->render_finished_semaphore,
      nullptr);
    vkDestroySemaphore(vk_state->device, frame_resources->image_available_semaphore,
      nullptr);
    vkDestroyFence(vk_state->device, frame_resources->frame_rendered_fence, nullptr);
  }

  vkDestroyCommandPool(vk_state->device, vk_state->command_pool, nullptr);
  vkDestroyDevice(vk_state->device, nullptr);
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger,
      nullptr);
  }
  vkDestroySurfaceKHR(vk_state->instance, vk_state->surface, nullptr);
  vkDestroyInstance(vk_state->instance, nullptr);
}


void vulkan::recreate_swapchain(VkState *vk_state, CommonState *common_state) {
  logs::info("Recreating swapchain");

  // If the width or height is 0, wait until they're both greater than zero.
  // We don't want to do anything while the window size is zero.
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(common_state->window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(common_state->window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(vk_state->device);

  destroy_swapchain(vk_state);

  init_swapchain_support_details(&vk_state->swapchain_support_details,
    vk_state->physical_device, vk_state->surface);
  init_swapchain(vk_state, common_state->window, &common_state->extent);
  init_render_pass(vk_state);
  init_framebuffers(vk_state, common_state->extent);
  init_pipeline(vk_state, common_state->extent);
  init_command_buffers(vk_state, common_state->extent);
}


void vulkan::render(VkState *vk_state, CommonState *common_state) {
  FrameResources *frame_resources = &vk_state->frame_resources[vk_state->idx_frame];

  vkWaitForFences(vk_state->device, 1, &frame_resources->frame_rendered_fence, VK_TRUE,
    UINT64_MAX);
  vkResetFences(vk_state->device, 1, &frame_resources->frame_rendered_fence);

  // Acquire image
  u32 idx_image;
  VkResult acquire_image_res = vkAcquireNextImageKHR(vk_state->device,
    vk_state->swapchain, UINT64_MAX, frame_resources->image_available_semaphore,
    VK_NULL_HANDLE, &idx_image);

  if (acquire_image_res == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain(vk_state, common_state);
    return;
  } else if (
    acquire_image_res != VK_SUCCESS && acquire_image_res != VK_SUBOPTIMAL_KHR
  ) {
    logs::fatal("Could not acquire swap chain image.");
  }

  VkSemaphore const wait_semaphores[] = {frame_resources->image_available_semaphore};
  VkPipelineStageFlags const wait_stages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkSemaphore const signal_semaphores[] = {frame_resources->render_finished_semaphore};

  // Update UBO
  void *memory;
  vkMapMemory(vk_state->device, frame_resources->uniform_buffer_memory, 0,
    sizeof(CoreSceneState), 0, &memory);
  memcpy(memory, &common_state->core_scene_state, sizeof(CoreSceneState));
  vkUnmapMemory(vk_state->device, frame_resources->uniform_buffer_memory);

  // Draw into image
  VkSubmitInfo const submit_info = {
    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = wait_semaphores,
    .pWaitDstStageMask    = wait_stages,
    .commandBufferCount   = 1,
    .pCommandBuffers      = &frame_resources->command_buffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = signal_semaphores,
  };

  if (
    vkQueueSubmit(vk_state->graphics_queue, 1, &submit_info,
      frame_resources->frame_rendered_fence) != VK_SUCCESS
  ) {
    logs::fatal("Could not submit draw command buffer.");
  }

  // Present image
  VkSwapchainKHR const swapchains[] = {vk_state->swapchain};
  VkPresentInfoKHR const present_info = {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = signal_semaphores,
    .swapchainCount     = 1,
    .pSwapchains        = swapchains,
    .pImageIndices      = &idx_image,
  };

  VkResult const present_res = vkQueuePresentKHR(vk_state->present_queue, &present_info);

  if (
    present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR ||
    vk_state->should_recreate_swapchain
  ) {
    recreate_swapchain(vk_state, common_state);
    vk_state->should_recreate_swapchain = false;
  } else if (present_res != VK_SUCCESS) {
    logs::fatal("Could not present swap chain image.");
  }

  vk_state->idx_frame = (vk_state->idx_frame + 1) % N_PARALLEL_FRAMES;
}


void vulkan::wait(VkState *vk_state) {
  vkQueueWaitIdle(vk_state->present_queue);
  vkDeviceWaitIdle(vk_state->device);
}
