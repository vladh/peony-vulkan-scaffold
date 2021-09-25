namespace vulkan::stage_common {
  static VkSampler guard_sampler(VkSampler sampler, VkSampler dummy_sampler) {
    if (sampler == VK_NULL_HANDLE) {
      return dummy_sampler;
    }
    return sampler;
  }


  static VkImageView guard_image_view(VkImageView image_view, VkImageView dummy_image_view) {
    if (image_view == VK_NULL_HANDLE) {
      return dummy_image_view;
    }
    return image_view;
  }
}
