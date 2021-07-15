#pragma once

#include <vulkan/vulkan.h>

struct VkState {
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
};

namespace vulkan {
  void init(VkState *vk_state);
  void destroy(VkState *vk_state);
}
