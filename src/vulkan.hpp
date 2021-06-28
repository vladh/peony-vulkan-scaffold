#pragma once

#include <vulkan/vulkan.h>

namespace vulkan {
  void init(VkInstance *instance);
  void destroy(VkInstance *instance);
}
