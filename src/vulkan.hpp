#pragma once

#include <optional>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "types.hpp"

constexpr u32 const MAX_N_SWAPCHAIN_FORMATS = 32;
constexpr u32 const MAX_N_SWAPCHAIN_PRESENT_MODES = 32;
constexpr u32 const MAX_N_SWAPCHAIN_IMAGES = 8;

struct QueueFamilyIndices {
  std::optional<u32> idx_graphics_family_queue;
  std::optional<u32> idx_present_family_queue;
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR formats[MAX_N_SWAPCHAIN_FORMATS];
  u32 n_formats;
  VkPresentModeKHR present_modes[MAX_N_SWAPCHAIN_PRESENT_MODES];
  u32 n_present_modes;
};

struct VkState {
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkPhysicalDevice physical_device;
  QueueFamilyIndices queue_family_indices;
  SwapChainSupportDetails swapchain_support_details;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[MAX_N_SWAPCHAIN_IMAGES];
  VkImageView swapchain_image_views[MAX_N_SWAPCHAIN_IMAGES];
  u32 n_swapchain_images;
  VkFormat swapchain_image_format;
  VkExtent2D swapchain_extent;
  VkPipelineLayout pipeline_layout;
};

namespace vulkan {
  void init(VkState *vk_state, GLFWwindow *window);
  void destroy(VkState *vk_state);
}
