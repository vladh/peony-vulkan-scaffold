#include <stdint.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "glm.hpp"
#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "files.hpp"

#include "vulkan_utils.cpp"
#include "vulkan_swapchain.cpp"
#include "vulkan_general.cpp"


//
// Descriptor stuff
//


static void init_descriptors(VkState *vk_state) {
  // Create uniform buffer
  // TODO: When we render multiple frames at the same time, we'll want to create
  // a separate UBO for each. Maybe.
  // vulkan-tutorial.com/Uniform_buffers/Descriptor_layout_and_buffer.html
  make_buffer(vk_state->device, vk_state->physical_device,
    sizeof(ShaderCommon),
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    &vk_state->uniform_buffer,
    &vk_state->uniform_buffer_memory);

  // Populate descriptors
  VkDescriptorBufferInfo const buffer_info = {
    .buffer = vk_state->uniform_buffer,
    .offset = 0,
    .range = sizeof(ShaderCommon),
  };
  VkDescriptorImageInfo const image_info = {
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .imageView = vk_state->texture_image_view,
    .sampler = vk_state->texture_sampler,
  };
  VkWriteDescriptorSet descriptor_writes[] = {
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = nullptr,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .pBufferInfo = &buffer_info,
    },
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = nullptr,
      .dstBinding = 1,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo = &image_info,
    },
  };

  make_descriptors(
    vk_state->device,
    descriptor_writes,
    LEN(descriptor_writes),
    &vk_state->descriptor_set_layout,
    &vk_state->descriptor_pool,
    &vk_state->descriptor_set
  );
}


static void update_ubo(VkState *vk_state) {
  ShaderCommon ubo = {
    .model = glm::rotate(
      glm::mat4(1.0f),
      0.0f * glm::radians(90.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)
    ),
    .view = glm::lookAt(
      glm::vec3(2.0f, 2.0f, 2.0f),
      glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)
    ),
    .projection = glm::perspective(
      glm::radians(45.0f),
      (f32)vk_state->swapchain_extent.width / (f32)vk_state->swapchain_extent.height,
      0.01f,
      20.0f
    ),
  };

  // In OpenGL (which GLM was designed for), the y coordinate of the clip coordinates
  // is inverted. This is not true in Vulkan, so we invert it back.
  ubo.projection[1][1] *= -1;

  void *memory;
  vkMapMemory(vk_state->device, vk_state->uniform_buffer_memory, 0,
    sizeof(ShaderCommon), 0, &memory);
  memcpy(memory, &ubo, sizeof(ShaderCommon));
  vkUnmapMemory(vk_state->device, vk_state->uniform_buffer_memory);
}


//
// Pipeline stuff
//


static VkShaderModule init_shader_module(
  VkState *vk_state, u8 const *shader, size_t size
) {
  VkShaderModuleCreateInfo const shader_module_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = size,
    .pCode = (u32*)shader,
  };

  VkShaderModule shader_module;
  if (
    vkCreateShaderModule(vk_state->device, &shader_module_info, nullptr,
      &shader_module) != VK_SUCCESS
  ) {
    logs::fatal("Could not create shader module.");
  }

  return shader_module;
}


static void init_render_pass(VkState *vk_state) {
  VkAttachmentDescription const color_attachment = {
    .format = vk_state->swapchain_image_format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };
  VkAttachmentReference const color_attachment_ref = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentDescription const depth_attachment = {
    .format = VK_FORMAT_D32_SFLOAT,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  VkAttachmentReference const depth_attachment_ref = {
    .attachment = 1,
    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription const subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment_ref,
    .pDepthStencilAttachment = &depth_attachment_ref,
  };
  VkSubpassDependency const dependency = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
  };

  VkAttachmentDescription const attachments[] = {color_attachment, depth_attachment};
  VkRenderPassCreateInfo const render_pass_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = LEN(attachments),
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,
  };

  if (
    vkCreateRenderPass(vk_state->device, &render_pass_info, nullptr,
      &vk_state->render_pass) != VK_SUCCESS
  ) {
    logs::fatal("Could not create render pass.");
  }
}


static void init_pipeline(VkState *vk_state) {
  // Pipeline layout
  VkPipelineLayoutCreateInfo const pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &vk_state->descriptor_set_layout,
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
  VkShaderModule const vert_shader_module = init_shader_module(vk_state,
    vert_shader, vert_shader_size);
  VkPipelineShaderStageCreateInfo const vert_shader_stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vert_shader_module,
    .pName = "main",
  };

  size_t frag_shader_size;
  u8 *frag_shader = files::load_file_to_pool_u8(&pool,
    "bin/shaders/test.frag.spv", &frag_shader_size);
  VkShaderModule const frag_shader_module = init_shader_module(vk_state,
    frag_shader, frag_shader_size);
  VkPipelineShaderStageCreateInfo const frag_shader_stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = frag_shader_module,
    .pName = "main",
  };

  VkPipelineShaderStageCreateInfo const shader_stages[] = {
    vert_shader_stage_info,
    frag_shader_stage_info,
  };

  // Pipeline
  VkPipelineVertexInputStateCreateInfo const vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &VERTEX_BINDING_DESCRIPTION,
    .vertexAttributeDescriptionCount = LEN(VERTEX_ATTRIBUTE_DESCRIPTIONS),
    .pVertexAttributeDescriptions = VERTEX_ATTRIBUTE_DESCRIPTIONS,
  };
  VkPipelineInputAssemblyStateCreateInfo const input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };
  VkViewport const viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = (f32)vk_state->swapchain_extent.width,
    .height = (f32)vk_state->swapchain_extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };
  VkRect2D const scissor = {
    .offset = {0, 0},
    .extent = vk_state->swapchain_extent,
  };
  VkPipelineViewportStateCreateInfo const viewport_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &viewport,
    .scissorCount = 1,
    .pScissors = &scissor,
  };
  VkPipelineRasterizationStateCreateInfo const rasterizer_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .lineWidth = 1.0f,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
  };
  VkPipelineMultisampleStateCreateInfo const multisampling_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };
  VkPipelineDepthStencilStateCreateInfo const depth_stencil_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
  };
  VkPipelineColorBlendAttachmentState color_blend_attachment = {
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
  };
  VkPipelineColorBlendStateCreateInfo const color_blending_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachment,
  };

  VkGraphicsPipelineCreateInfo const pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shader_stages,
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_state_info,
    .pRasterizationState = &rasterizer_info,
    .pMultisampleState = &multisampling_info,
    .pDepthStencilState = &depth_stencil_info,
    .pColorBlendState = &color_blending_info,
    .pDynamicState = nullptr,
    .layout = vk_state->pipeline_layout,
    .renderPass = vk_state->render_pass,
    .subpass = 0,
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


//
// Specific image buffers
//


static void init_depth_buffer(VkState *vk_state) {
  make_image(vk_state->device, vk_state->physical_device,
    &vk_state->depth_image, &vk_state->depth_image_memory,
    vk_state->swapchain_extent.width, vk_state->swapchain_extent.height,
    VK_FORMAT_D32_SFLOAT,
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vk_state->depth_image_view = make_image_view(vk_state->device,
    vk_state->depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}


static void init_framebuffers(VkState *vk_state) {
  range (0, vk_state->n_swapchain_images) {
    VkImageView const attachments[] = {
      vk_state->swapchain_image_views[idx],
      vk_state->depth_image_view,
    };
    VkFramebufferCreateInfo const framebuffer_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = vk_state->render_pass,
      .attachmentCount = LEN(attachments),
      .pAttachments = attachments,
      .width = vk_state->swapchain_extent.width,
      .height = vk_state->swapchain_extent.height,
      .layers = 1,
    };
    if (
      vkCreateFramebuffer(vk_state->device, &framebuffer_info, nullptr,
        &vk_state->swapchain_framebuffers[idx]) != VK_SUCCESS
    ) {
      logs::fatal("Could not create framebuffer.");
    }
  }
}


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
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = vk_state->physical_device_properties.limits.maxSamplerAnisotropy,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .mipLodBias = 0.0f,
    .minLod = 0.0f,
    .maxLod = 0.0f,
  };
  if (
    vkCreateSampler(vk_state->device, &sampler_info, nullptr,
      &vk_state->texture_sampler) != VK_SUCCESS
  ) {
    logs::fatal("Could not create texture sampler.");
  }
}


//
// Data (vertex/index/ubo etc.) buffers
//


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


//
// Command buffers
//


static void init_command_buffers(VkState *vk_state) {
  VkCommandBufferAllocateInfo const alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = vk_state->command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = vk_state->n_swapchain_images,
  };

  if (
    vkAllocateCommandBuffers(vk_state->device, &alloc_info, vk_state->command_buffers)
    != VK_SUCCESS
  ) {
    logs::fatal("Could not allocate command buffers.");
  }

  range (0, vk_state->n_swapchain_images) {
    VkCommandBuffer const command_buffer = vk_state->command_buffers[idx];

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
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = vk_state->render_pass,
      .framebuffer = vk_state->swapchain_framebuffers[idx],
      .renderArea.offset = {0, 0},
      .renderArea.extent = vk_state->swapchain_extent,
      .clearValueCount = LEN(clear_colors),
      .pClearValues = clear_colors,
    };

    vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk_state->pipeline);

    VkBuffer const vertex_buffers[] = {vk_state->vertex_buffer};
    VkDeviceSize const offsets[] = {0};
    vkCmdBindVertexBuffers(vk_state->command_buffers[idx], 0, 1,
      vertex_buffers, offsets);
    vkCmdBindIndexBuffer(vk_state->command_buffers[idx],
      vk_state->index_buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk_state->pipeline_layout, 0, 1, &vk_state->descriptor_set, 0, nullptr);
    vkCmdDrawIndexed(command_buffer, LEN(INDICES), 1, 0, 0, 0);
    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
      logs::fatal("Could not record command buffer.");
    }
  }
}


//
// General init/destroy etc.
//


static void init_semaphores(VkState *vk_state) {
  VkSemaphoreCreateInfo const semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  if (
    vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
      &vk_state->image_available_semaphore) != VK_SUCCESS ||
    vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
      &vk_state->render_finished_semaphore) != VK_SUCCESS
  ) {
    logs::fatal("Could not create semaphores.");
  }
}


void vulkan::init(VkState *vk_state, GLFWwindow *window) {
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
  init_surface(vk_state, window);
  init_physical_device(vk_state);
  init_logical_device(vk_state);
  init_swapchain(vk_state, window);
  init_render_pass(vk_state);
  init_depth_buffer(vk_state);
  init_framebuffers(vk_state);
  init_command_pool(vk_state);
  init_textures(vk_state);
  init_descriptors(vk_state);
  init_buffers(vk_state);
  init_pipeline(vk_state);
  init_command_buffers(vk_state);
  init_semaphores(vk_state);
}


static void destroy_swapchain(VkState *vk_state) {
  vkDestroyImageView(vk_state->device, vk_state->depth_image_view, nullptr);
  vkDestroyImage(vk_state->device, vk_state->depth_image, nullptr);
  vkFreeMemory(vk_state->device, vk_state->depth_image_memory, nullptr);

  range (0, vk_state->n_swapchain_images) {
    vkDestroyFramebuffer(vk_state->device, vk_state->swapchain_framebuffers[idx],
      nullptr);
  }
  vkFreeCommandBuffers(vk_state->device, vk_state->command_pool,
    vk_state->n_swapchain_images, vk_state->command_buffers);
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

  vkDestroyBuffer(vk_state->device, vk_state->uniform_buffer, nullptr);
  vkFreeMemory(vk_state->device, vk_state->uniform_buffer_memory, nullptr);
  vkDestroyDescriptorPool(vk_state->device, vk_state->descriptor_pool, nullptr);

  vkDestroySemaphore(vk_state->device, vk_state->render_finished_semaphore, nullptr);
  vkDestroySemaphore(vk_state->device, vk_state->image_available_semaphore, nullptr);

  vkDestroyCommandPool(vk_state->device, vk_state->command_pool, nullptr);
  vkDestroyDevice(vk_state->device, nullptr);
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger,
      nullptr);
  }
  vkDestroySurfaceKHR(vk_state->instance, vk_state->surface, nullptr);
  vkDestroyInstance(vk_state->instance, nullptr);
}


void vulkan::recreate_swapchain(VkState *vk_state, GLFWwindow *window) {
  logs::info("Recreating swapchain");

  // If the width or height is 0, wait until they're both greater than zero.
  // We don't want to do anything while the window size is zero.
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(vk_state->device);

  destroy_swapchain(vk_state);

  init_swapchain_support_details(&vk_state->swapchain_support_details,
    vk_state->physical_device, vk_state->surface);
  init_swapchain(vk_state, window);
  init_render_pass(vk_state);
  init_pipeline(vk_state);
  init_depth_buffer(vk_state);
  init_framebuffers(vk_state);
  init_command_buffers(vk_state);
}


void vulkan::render(VkState *vk_state, GLFWwindow *window) {
  u32 idx_image;
  VkResult acquire_image_res = vkAcquireNextImageKHR(vk_state->device,
    vk_state->swapchain, UINT64_MAX, vk_state->image_available_semaphore, VK_NULL_HANDLE,
    &idx_image);

  if (acquire_image_res == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain(vk_state, window);
    return;
  } else if (acquire_image_res != VK_SUCCESS && acquire_image_res != VK_SUBOPTIMAL_KHR) {
    logs::fatal("Could not acquire swap chain image.");
  }

  VkSemaphore const wait_semaphores[] = {vk_state->image_available_semaphore};
  VkPipelineStageFlags const wait_stages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkSemaphore const signal_semaphores[] = {vk_state->render_finished_semaphore};

  update_ubo(vk_state);

  VkSubmitInfo const submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = wait_semaphores,
    .pWaitDstStageMask = wait_stages,
    .commandBufferCount = 1,
    .pCommandBuffers = &vk_state->command_buffers[idx_image],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signal_semaphores,
  };

  if (
    vkQueueSubmit(vk_state->graphics_queue, 1, &submit_info,
      VK_NULL_HANDLE) != VK_SUCCESS
  ) {
    logs::fatal("Could not submit draw command buffer.");
  }

  VkSwapchainKHR const swapchains[] = {vk_state->swapchain};
  VkPresentInfoKHR const present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signal_semaphores,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices = &idx_image,
  };

  VkResult const present_res = vkQueuePresentKHR(vk_state->present_queue, &present_info);

  if (
    present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR ||
    vk_state->should_recreate_swapchain
  ) {
    recreate_swapchain(vk_state, window);
    vk_state->should_recreate_swapchain = false;
  } else if (present_res != VK_SUCCESS) {
    logs::fatal("Could not present swap chain image.");
  }
}


void vulkan::wait(VkState *vk_state) {
  vkQueueWaitIdle(vk_state->present_queue);
  vkDeviceWaitIdle(vk_state->device);
}
