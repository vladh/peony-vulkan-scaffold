/*
  Functions that use Vulkan to interact with the GPU and eventually render stuff
  on screen.
*/


#include <stdint.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"
#include <vector>

#include "glm.hpp"
#include "common.hpp"
#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "files.hpp"

#include "vkutils.cpp"
#include "vulkan_swapchain.cpp"
#include "vulkan_resources.cpp"
#include "vulkan_general.cpp"
#include "vulkan_rendering.cpp"
#include "vulkan_stage_geometry.cpp"
#include "vulkan_stage_lighting.cpp"
#include "vulkan_stage_forward.cpp"


static void init_synchronization(VkState *vk_state) {
  VkSemaphoreCreateInfo const semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkutils::check(vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
      &frame_resources->image_available_semaphore));
    vkutils::check(vkCreateFence(vk_state->device, &fence_info, nullptr, &frame_resources->frame_rendered_fence));
  }
}


void vulkan::init(VkState *vk_state, CommonState *common_state) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};

  if (USE_VALIDATION) {
    if (!ensure_validation_layers_supported()) {
      logs::fatal("Could not get required validation layers.");
    }

    debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = debug_callback;
  }

  init_instance(vk_state, &debug_messenger_info);
  init_debug_messenger(vk_state, &debug_messenger_info);
  init_surface(vk_state, common_state->window);
  init_physical_device(vk_state);
  init_logical_device(vk_state);
  init_swapchain(vk_state, common_state->window, &common_state->extent);

  // We only create one command pool which we use for everything graphics-related
  vkutils::create_command_pool(vk_state->device, &vk_state->command_pool, (u32)vk_state->queue_family_indices.graphics);
  init_textures(vk_state);
  init_buffers(vk_state);
  init_uniform_buffers(vk_state);

  init_geometry_stage(vk_state, common_state->extent);
  init_lighting_stage(vk_state, common_state->extent);
  init_forward_stage(vk_state, common_state->extent);

  init_synchronization(vk_state);
}


static void destroy_swapchain(VkState *vk_state) {
  vkutils::destroy_image_resources(vk_state->device, &vk_state->depthbuffer);
  vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->g_position);
  vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->g_normal);
  vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->g_albedo);
  vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->g_pbr);

  vkDestroySwapchainKHR(vk_state->device, vk_state->swapchain, nullptr);

  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkDestroyBuffer(vk_state->device, frame_resources->uniform_buffer, nullptr);
    vkFreeMemory(vk_state->device, frame_resources->uniform_buffer_memory, nullptr);
  }

  destroy_geometry_stage_swapchain(vk_state);
  destroy_lighting_stage_swapchain(vk_state);
  destroy_forward_stage_swapchain(vk_state);

  range (0, vk_state->n_swapchain_images) {
    vkDestroyImageView(vk_state->device, vk_state->swapchain_image_views[idx], nullptr);
  }
}


void vulkan::destroy(VkState *vk_state) {
  destroy_swapchain(vk_state);

  vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->alpaca);

  vkutils::destroy_buffer_resources(vk_state->device, &vk_state->sign.vertex);
  vkutils::destroy_buffer_resources(vk_state->device, &vk_state->fsign.vertex);
  vkutils::destroy_buffer_resources(vk_state->device, &vk_state->screenquad.vertex);
  vkutils::destroy_buffer_resources(vk_state->device, &vk_state->sign.index);
  vkutils::destroy_buffer_resources(vk_state->device, &vk_state->fsign.index);
  vkutils::destroy_buffer_resources(vk_state->device, &vk_state->screenquad.index);

  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkDestroySemaphore(vk_state->device, frame_resources->image_available_semaphore, nullptr);
    vkDestroyFence(vk_state->device, frame_resources->frame_rendered_fence, nullptr);
  }

  destroy_geometry_stage_nonswapchain(vk_state);
  destroy_lighting_stage_nonswapchain(vk_state);
  destroy_forward_stage_nonswapchain(vk_state);

  vkDestroyCommandPool(vk_state->device, vk_state->command_pool, nullptr);
  vkDestroyDevice(vk_state->device, nullptr);
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger, nullptr);
  }
  vkDestroySurfaceKHR(vk_state->instance, vk_state->surface, nullptr);
  vkDestroyInstance(vk_state->instance, nullptr);
}


void vulkan::recreate_swapchain(VkState *vk_state, CommonState *common_state) {
  logs::info("Recreating swapchain");

  // If the width or height is 0, wait until they're both greater than zero.
  // We don't want to do anything while the window size is zero.
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(common_state->window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(common_state->window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(vk_state->device);

  destroy_swapchain(vk_state);

  init_swapchain_support_details(&vk_state->swapchain_support_details, vk_state->physical_device, vk_state->surface);
  init_swapchain(vk_state, common_state->window, &common_state->extent);
  init_uniform_buffers(vk_state);

  init_geometry_stage_swapchain(vk_state, common_state->extent);
  init_lighting_stage_swapchain(vk_state, common_state->extent);
  init_forward_stage_swapchain(vk_state, common_state->extent);
}


void vulkan::render(VkState *vk_state, CommonState *common_state) {
  FrameResources *frame_resources = &vk_state->frame_resources[vk_state->idx_frame];

  vkWaitForFences(vk_state->device, 1, &frame_resources->frame_rendered_fence, VK_TRUE, UINT64_MAX);

  // Update UBO
  void *memory;
  vkMapMemory(vk_state->device, frame_resources->uniform_buffer_memory, 0, sizeof(CoreSceneState), 0, &memory);
  memcpy(memory, &common_state->core_scene_state, sizeof(CoreSceneState));
  vkUnmapMemory(vk_state->device, frame_resources->uniform_buffer_memory);

  // Acquire image
  u32 idx_image;
  {
    VkResult acquire_image_res = vkAcquireNextImageKHR(vk_state->device, vk_state->swapchain, UINT64_MAX,
      frame_resources->image_available_semaphore, VK_NULL_HANDLE, &idx_image);

    if (acquire_image_res == VK_ERROR_OUT_OF_DATE_KHR) {
      recreate_swapchain(vk_state, common_state);
      return;
    } else if (acquire_image_res != VK_SUCCESS && acquire_image_res != VK_SUBOPTIMAL_KHR) {
      logs::fatal("Could not acquire swap chain image.");
    }
  }

  // Render each stage
  render_geometry_stage(vk_state, common_state->extent, idx_image);
  render_lighting_stage(vk_state, common_state->extent, idx_image);
  render_forward_stage(vk_state, common_state->extent, idx_image);

  // Present image
  {
    VkSemaphore const signal_semaphores[] = {vk_state->forward_stage.render_finished_semaphore};
    VkSwapchainKHR const swapchains[] = {vk_state->swapchain};
    VkPresentInfoKHR const present_info = {
      .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores    = signal_semaphores,
      .swapchainCount     = 1,
      .pSwapchains        = swapchains,
      .pImageIndices      = &idx_image,
    };
    VkResult const present_res = vkQueuePresentKHR(vk_state->present_queue, &present_info);

    if (
      present_res == VK_ERROR_OUT_OF_DATE_KHR ||
      present_res == VK_SUBOPTIMAL_KHR || vk_state->should_recreate_swapchain
    ) {
      recreate_swapchain(vk_state, common_state);
      vk_state->should_recreate_swapchain = false;
    } else if (present_res != VK_SUCCESS) {
      logs::fatal("Could not present swap chain image.");
    }
  }

  vk_state->idx_frame = (vk_state->idx_frame + 1) % N_PARALLEL_FRAMES;
}


void vulkan::wait(VkState *vk_state) {
  vkQueueWaitIdle(vk_state->present_queue);
  vkDeviceWaitIdle(vk_state->device);
}
