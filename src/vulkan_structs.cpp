/*
  Shorthands functions to create some common Vulkan structs. They should not
  create any resources.
*/


static VkSamplerCreateInfo sampler_create_info(
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
    .maxAnisotropy           = physical_device_props.limits.maxSamplerAnisotropy,
    .compareEnable           = VK_FALSE,
    .compareOp               = VK_COMPARE_OP_ALWAYS,
    .minLod                  = 0.0f,
    .maxLod                  = 0.0f,
    .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };
}


static VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state() {
  return {
    .blendEnable         = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp        = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp        = VK_BLEND_OP_ADD,
    .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
}


static VkAttachmentReference attachment_reference(u32 attachment, VkImageLayout layout) {
  return {
    .attachment = attachment,
    .layout     = layout,
  };
}


static VkAttachmentDescription attachment_description(
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


static VkCommandBufferBeginInfo command_buffer_begin_info() {
  return {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
}


static VkCommandBufferAllocateInfo command_buffer_allocate_info(
  VkCommandPool commandPool
) {
  return {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = commandPool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
}


static VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info(
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


static VkPipelineLayoutCreateInfo pipeline_layout_create_info(
  const VkDescriptorSetLayout* pSetLayouts
) {
  return {
    .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts    = pSetLayouts,
  };
}


static VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info_vert(
  VkShaderModule shader_module
) {
  return {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage  = VK_SHADER_STAGE_VERTEX_BIT,
    .module = shader_module,
    .pName  = "main",
  };
}


static VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info_frag(
  VkShaderModule shader_module
) {
  return {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = shader_module,
    .pName  = "main",
  };
}


static VkDescriptorSetAllocateInfo descriptor_set_allocate_info(
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


static VkWriteDescriptorSet write_descriptor_set_buffer(
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


static VkWriteDescriptorSet write_descriptor_set_image(
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


static VkDescriptorPoolSize descriptor_pool_size(
  VkDescriptorType type,
  u32 descriptorCount
) {
  return {
    .type            = type,
    .descriptorCount = descriptorCount,
  };
}


static VkDescriptorPoolCreateInfo descriptor_pool_create_info(
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


static VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info(
  u32 bindingCount,
  const VkDescriptorSetLayoutBinding* pBindings
) {
  return {
    .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = bindingCount,
    .pBindings    = pBindings,
  };
}


static VkDescriptorSetLayoutBinding descriptor_set_layout_binding(
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


static VkDescriptorSetLayoutBinding descriptor_set_layout_binding(
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
