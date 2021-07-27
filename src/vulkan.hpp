#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "types.hpp"

struct ShaderCommon {
  m4 model;
  m4 view;
  m4 projection;
};

struct Vertex {
  v3 position;
  v3 color;
};

static constexpr i64 const NO_QUEUE_FAMILY = -1;
static constexpr u32 const MAX_N_SWAPCHAIN_FORMATS = 32;
static constexpr u32 const MAX_N_SWAPCHAIN_PRESENT_MODES = 32;
static constexpr u32 const MAX_N_SWAPCHAIN_IMAGES = 8;
constexpr u32 const MAX_N_REQUIRED_EXTENSIONS = 256;

constexpr bool const USE_VALIDATION = true;
constexpr char const * const VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
constexpr char const * const REQUIRED_DEVICE_EXTENSIONS[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  #if PLATFORM & PLATFORM_MACOS
    "VK_KHR_portability_subset",
  #endif
};

constexpr Vertex const VERTICES[] = {
  {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
  {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
  {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
  {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}},
};

constexpr u32 INDICES[] = {0, 1, 2, 2, 3, 0};

constexpr VkVertexInputBindingDescription const VERTEX_BINDING_DESCRIPTION = {
  .binding = 0,
  .stride = sizeof(Vertex),
  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};
constexpr VkVertexInputAttributeDescription const VERTEX_ATTRIBUTE_DESCRIPTIONS[2] = {
  {
    .binding = 0,
    .location = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(Vertex, position),
  },
  {
    .binding = 0,
    .location = 1,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(Vertex, color),
  }
};

struct QueueFamilyIndices {
  i64 graphics;
  i64 present;
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR formats[MAX_N_SWAPCHAIN_FORMATS];
  u32 n_formats; VkPresentModeKHR present_modes[MAX_N_SWAPCHAIN_PRESENT_MODES];
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
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkCommandPool command_pool;
  VkCommandBuffer command_buffers[MAX_N_SWAPCHAIN_IMAGES];
  VkSemaphore image_available;
  VkSemaphore render_finished;
  bool should_recreate_swapchain;
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_buffer_memory;
  VkBuffer index_buffer;
  VkDeviceMemory index_buffer_memory;
  VkBuffer uniform_buffer;
  VkDeviceMemory uniform_buffer_memory;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;
};

namespace vulkan {
  void init(VkState *vk_state, GLFWwindow *window);
  void recreate_swapchain(VkState *vk_state, GLFWwindow *window);
  void destroy(VkState *vk_state);
  void render(VkState *vk_state, GLFWwindow *window);
  void wait(VkState *vk_state);
}
