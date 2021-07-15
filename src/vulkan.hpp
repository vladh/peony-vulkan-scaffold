#pragma once

#include <optional>
#include <vulkan/vulkan.h>
#include "types.hpp"

struct QueueFamilyIndices {
  std::optional<u32> idx_graphics_family_queue;
};

struct VkState {
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkPhysicalDevice physical_device;
  QueueFamilyIndices queue_family_indices;
  VkDevice device;
  VkQueue graphics_queue;
};

namespace vulkan {
  void init(VkState *vk_state);
  void destroy(VkState *vk_state);
}
