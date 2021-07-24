#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "types.hpp"

static constexpr i64 const NO_QUEUE_FAMILY = -1;
static constexpr u32 const MAX_N_SWAPCHAIN_FORMATS = 32;
static constexpr u32 const MAX_N_SWAPCHAIN_PRESENT_MODES = 32;
static constexpr u32 const MAX_N_SWAPCHAIN_IMAGES = 8;

struct QueueFamilyIndices {
  i64 graphics;
  i64 present;
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
  VkFramebuffer swapchain_framebuffers[MAX_N_SWAPCHAIN_IMAGES];
  u32 n_swapchain_images;
  VkFormat swapchain_image_format;
  VkExtent2D swapchain_extent;
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkCommandPool command_pool;
  VkCommandBuffer command_buffers[MAX_N_SWAPCHAIN_IMAGES];
  VkSemaphore image_available;
  VkSemaphore render_finished;
};

namespace vulkan {
  void init(VkState *vk_state, GLFWwindow *window);
  void destroy(VkState *vk_state);
  void render(VkState *vk_state);
  void wait(VkState *vk_state);
}
