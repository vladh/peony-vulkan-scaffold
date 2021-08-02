/*
  Shorthands functions to create some common Vulkan structs. They should not create
  any resources.
*/


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
    .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
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
    .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 1,
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
    .poolSizeCount = poolSizeCount,
    .pPoolSizes    = pPoolSizes,
    .maxSets       = maxSets,
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
    .descriptorCount = 1,
    .stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS,
    .descriptorType  = descriptorType,
  };
}


static VkDescriptorSetLayoutBinding descriptor_set_layout_binding(
  u32 binding,
  VkDescriptorType descriptorType,
  VkShaderStageFlags stageFlags
) {
  return {
    .binding         = binding,
    .descriptorCount = 1,
    .stageFlags      = stageFlags,
    .descriptorType  = descriptorType,
  };
}
