#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "types.hpp"
#include "common.hpp"

struct Vertex {
  v3 position;
  v3 normal;
  v2 tex_coords;
};

static constexpr i64 NO_QUEUE_FAMILY               = -1;
static constexpr u32 MAX_N_SWAPCHAIN_FORMATS       = 32;
static constexpr u32 MAX_N_SWAPCHAIN_PRESENT_MODES = 32;
static constexpr u32 MAX_N_SWAPCHAIN_IMAGES        = 8;
static constexpr u32 N_PARALLEL_FRAMES             = 3;
static constexpr u32 MAX_N_REQUIRED_EXTENSIONS     = 256;

static constexpr bool USE_VALIDATION = true;
static constexpr char const *VALIDATION_LAYERS[] = {
  "VK_LAYER_KHRONOS_validation"
};
static constexpr char const *REQUIRED_DEVICE_EXTENSIONS[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  #if PLATFORM & PLATFORM_MACOS
    "VK_KHR_portability_subset",
  #endif
};

static constexpr Vertex SIGN_VERTICES[] = {
  {{-0.5f, -0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
  {{ 0.5f, -0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
  {{ 0.5f,  0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
  {{-0.5f,  0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},

  {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
  {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
  {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
  {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
};

static constexpr u32 SIGN_INDICES[] = {
  0, 1, 2, 2, 3, 0,
  4, 5, 6, 6, 7, 4,
};

static constexpr Vertex SCREENQUAD_VERTICES[] = {
  {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
  {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
  {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
  {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
};

static constexpr u32 SCREENQUAD_INDICES[] = {
  // TODO: Change face culling direction for everything!!!
  /* 0, 1, 2, 0, 2, 3, */
  2, 1, 0, 3, 2, 0
};

static constexpr VkVertexInputBindingDescription VERTEX_BINDING_DESCRIPTION = {
  .binding   = 0,
  .stride    = sizeof(Vertex),
  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};
static constexpr VkVertexInputAttributeDescription
VERTEX_ATTRIBUTE_DESCRIPTIONS[] = {
  {
    .location = 0,
    .binding  = 0,
    .format   = VK_FORMAT_R32G32B32_SFLOAT,
    .offset   = offsetof(Vertex, position),
  },
  {
    .location = 1,
    .binding  = 0,
    .format   = VK_FORMAT_R32G32B32_SFLOAT,
    .offset   = offsetof(Vertex, normal),
  },
  {
    .location = 2,
    .binding  = 0,
    .format   = VK_FORMAT_R32G32_SFLOAT,
    .offset   = offsetof(Vertex, tex_coords),
  }
};

struct QueueFamilyIndices {
  i64 graphics;
  i64 present;
};

struct SwapchainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR formats[MAX_N_SWAPCHAIN_FORMATS];
  u32 n_formats;
  VkPresentModeKHR present_modes[MAX_N_SWAPCHAIN_PRESENT_MODES];
  u32 n_present_modes;
};

struct FrameResources {
  VkSemaphore image_available_semaphore;
  VkFence frame_rendered_fence;

  VkBuffer uniform_buffer;
  VkDeviceMemory uniform_buffer_memory;

  VkDescriptorSet deferred_descriptor_set;
  VkCommandBuffer deferred_command_buffer;

  VkDescriptorSet main_descriptor_set;
  VkCommandBuffer main_command_buffer;
};

struct RenderStage {
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkFramebuffer framebuffers[MAX_N_SWAPCHAIN_IMAGES];
  VkSemaphore render_finished_semaphore;
};

struct ImageResources {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkSampler sampler;
};

struct VkState {
  // General Vulkan stuff
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceProperties physical_device_properties;
  QueueFamilyIndices queue_family_indices;
  SwapchainSupportDetails swapchain_support_details;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkSurfaceKHR surface;
  VkCommandPool command_pool;

  // Swapchain stuff
  VkSwapchainKHR swapchain;
  VkImageView swapchain_image_views[MAX_N_SWAPCHAIN_IMAGES];
  u32 n_swapchain_images;
  VkFormat swapchain_image_format;
  bool should_recreate_swapchain;

  // Frame resources
  FrameResources frame_resources[N_PARALLEL_FRAMES];

  // Scene resources
  VkBuffer sign_vertex_buffer;
  VkDeviceMemory sign_vertex_buffer_memory;
  VkBuffer screenquad_vertex_buffer;
  VkDeviceMemory screenquad_vertex_buffer_memory;

  VkBuffer sign_index_buffer;
  VkDeviceMemory sign_index_buffer_memory;
  VkBuffer screenquad_index_buffer;
  VkDeviceMemory screenquad_index_buffer_memory;

  VkImage texture_image;
  VkDeviceMemory texture_image_memory;
  VkImageView texture_image_view;
  VkSampler texture_sampler;

  // Rendering resources and information
  u32 idx_frame;
  ImageResources depthbuffer;
  ImageResources g_position;
  ImageResources g_normal;
  ImageResources g_albedo;
  ImageResources g_pbr;

  // Render stages
  RenderStage deferred_stage;
  RenderStage main_stage;
};

namespace vulkan {
  void init(VkState *vk_state, CommonState *common_state);
  void recreate_swapchain(VkState *vk_state, CommonState *common_state);
  void destroy(VkState *vk_state);
  void render(VkState *vk_state, CommonState *common_state);
  void wait(VkState *vk_state);
}
