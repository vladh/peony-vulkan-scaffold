/*
  Functions that use Vulkan to interact with the GPU and eventually render stuff
  on screen.
*/


#include <stdint.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "glm.hpp"
#include "common.hpp"
#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "files.hpp"

#include "vulkan_utils.cpp"
#include "vulkan_structs.cpp"
#include "vulkan_swapchain.cpp"
#include "vulkan_resources.cpp"
#include "vulkan_stage_deferred.cpp"
#include "vulkan_stage_main.cpp"
#include "vulkan_general.cpp"


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
    check_vk_result(vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
      &frame_resources->image_available_semaphore));
    check_vk_result(vkCreateFence(vk_state->device, &fence_info, nullptr,
      &frame_resources->frame_rendered_fence));
  }

  init_deferred_synchronization(vk_state);
  init_main_synchronization(vk_state);
}


void vulkan::init(VkState *vk_state, CommonState *common_state) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};

  if (USE_VALIDATION) {
    if (!ensure_validation_layers_supported()) {
      logs::fatal("Could not get required validation layers.");
    }

    debug_messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity =
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | */
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
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

  init_command_pool(vk_state);
  init_textures(vk_state);
  init_buffers(vk_state);

  // Deferred stage
  init_deferred_descriptor_set_layout(vk_state);
  init_deferred_descriptors(vk_state);
  init_deferred_render_pass(vk_state);
  init_deferred_framebuffers(vk_state, common_state->extent);
  init_deferred_pipeline(vk_state, common_state->extent);
  init_deferred_command_buffer(vk_state, common_state->extent);

  // Main stage
  init_main_descriptor_set_layout(vk_state);
  init_main_descriptors(vk_state);
  init_main_render_pass(vk_state);
  init_main_framebuffers(vk_state, common_state->extent);
  init_main_pipeline(vk_state, common_state->extent);
  init_main_command_buffer(vk_state, common_state->extent);

  init_synchronization(vk_state);
}


static void destroy_swapchain(VkState *vk_state) {
  destroy_image_resources(&vk_state->depthbuffer, vk_state->device);
  destroy_image_resources(&vk_state->g_position, vk_state->device);
  destroy_image_resources(&vk_state->g_normal, vk_state->device);
  destroy_image_resources(&vk_state->g_albedo, vk_state->device);
  destroy_image_resources(&vk_state->g_pbr, vk_state->device);

  vkDestroySwapchainKHR(vk_state->device, vk_state->swapchain, nullptr);

  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkDestroyBuffer(vk_state->device,
      frame_resources->uniform_buffer, nullptr);
    vkFreeMemory(vk_state->device,
      frame_resources->uniform_buffer_memory, nullptr);
  }

  // Deferred stage
  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkFreeCommandBuffers(vk_state->device, vk_state->command_pool,
      1, &frame_resources->deferred_command_buffer);
  }
  vkDestroyDescriptorPool(vk_state->device,
    vk_state->deferred_stage.descriptor_pool, nullptr);
  range (0, vk_state->n_swapchain_images) {
    vkDestroyFramebuffer(vk_state->device,
      vk_state->deferred_stage.framebuffers[idx], nullptr);
  }
  vkDestroyPipeline(vk_state->device, vk_state->deferred_stage.pipeline,
    nullptr);
  vkDestroyPipelineLayout(vk_state->device,
    vk_state->deferred_stage.pipeline_layout, nullptr);
  vkDestroyRenderPass(vk_state->device, vk_state->deferred_stage.render_pass,
    nullptr);

  // Main stage
  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkFreeCommandBuffers(vk_state->device, vk_state->command_pool,
      1, &frame_resources->main_command_buffer);
  }
  vkDestroyDescriptorPool(vk_state->device,
    vk_state->main_stage.descriptor_pool, nullptr);
  range (0, vk_state->n_swapchain_images) {
    vkDestroyFramebuffer(vk_state->device,
      vk_state->main_stage.framebuffers[idx], nullptr);
  }
  vkDestroyPipeline(vk_state->device, vk_state->main_stage.pipeline,
    nullptr);
  vkDestroyPipelineLayout(vk_state->device,
    vk_state->main_stage.pipeline_layout, nullptr);
  vkDestroyRenderPass(vk_state->device, vk_state->main_stage.render_pass,
    nullptr);

  range (0, vk_state->n_swapchain_images) {
    vkDestroyImageView(vk_state->device, vk_state->swapchain_image_views[idx],
      nullptr);
  }
}


void vulkan::destroy(VkState *vk_state) {
  destroy_swapchain(vk_state);

  vkDestroySampler(vk_state->device, vk_state->texture_sampler, nullptr);
  vkDestroyImageView(vk_state->device, vk_state->texture_image_view, nullptr);
  vkDestroyImage(vk_state->device, vk_state->texture_image, nullptr);
  vkFreeMemory(vk_state->device, vk_state->texture_image_memory, nullptr);

  vkDestroyBuffer(vk_state->device, vk_state->vertex_buffer, nullptr);
  vkFreeMemory(vk_state->device, vk_state->vertex_buffer_memory, nullptr);
  vkDestroyBuffer(vk_state->device, vk_state->index_buffer, nullptr);
  vkFreeMemory(vk_state->device, vk_state->index_buffer_memory, nullptr);

  range (0, N_PARALLEL_FRAMES) {
    FrameResources *frame_resources = &vk_state->frame_resources[idx];
    vkDestroySemaphore(vk_state->device,
      frame_resources->image_available_semaphore, nullptr);
    vkDestroyFence(vk_state->device, frame_resources->frame_rendered_fence,
      nullptr);
  }

  // Deferred stage
  vkDestroyDescriptorSetLayout(vk_state->device,
    vk_state->deferred_stage.descriptor_set_layout, nullptr);
  vkDestroySemaphore(vk_state->device,
    vk_state->deferred_stage.render_finished_semaphore, nullptr);

  // Main stage
  vkDestroyDescriptorSetLayout(vk_state->device,
    vk_state->main_stage.descriptor_set_layout, nullptr);
  vkDestroySemaphore(vk_state->device,
    vk_state->main_stage.render_finished_semaphore, nullptr);

  vkDestroyCommandPool(vk_state->device, vk_state->command_pool,
    nullptr);
  vkDestroyDevice(vk_state->device, nullptr);
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger,
      nullptr);
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

  init_swapchain_support_details(&vk_state->swapchain_support_details,
    vk_state->physical_device, vk_state->surface);
  init_swapchain(vk_state, common_state->window, &common_state->extent);

  // Deferred stage
  init_deferred_command_buffer(vk_state, common_state->extent);
  init_deferred_descriptors(vk_state);
  init_deferred_render_pass(vk_state);
  init_deferred_framebuffers(vk_state, common_state->extent);
  init_deferred_pipeline(vk_state, common_state->extent);

  // Main stage
  init_main_command_buffer(vk_state, common_state->extent);
  init_main_descriptors(vk_state);
  init_main_render_pass(vk_state);
  init_main_framebuffers(vk_state, common_state->extent);
  init_main_pipeline(vk_state, common_state->extent);
}


void vulkan::render(VkState *vk_state, CommonState *common_state) {
  FrameResources *frame_resources =
    &vk_state->frame_resources[vk_state->idx_frame];

  vkWaitForFences(vk_state->device, 1, &frame_resources->frame_rendered_fence,
    VK_TRUE, UINT64_MAX);

  // Update UBO
  void *memory;
  vkMapMemory(vk_state->device, frame_resources->uniform_buffer_memory, 0,
    sizeof(CoreSceneState), 0, &memory);
  memcpy(memory, &common_state->core_scene_state, sizeof(CoreSceneState));
  vkUnmapMemory(vk_state->device, frame_resources->uniform_buffer_memory);

  // Acquire image
  u32 idx_image;
  {
    VkResult acquire_image_res = vkAcquireNextImageKHR(vk_state->device,
      vk_state->swapchain, UINT64_MAX, frame_resources->image_available_semaphore,
      VK_NULL_HANDLE, &idx_image);

    if (acquire_image_res == VK_ERROR_OUT_OF_DATE_KHR) {
      recreate_swapchain(vk_state, common_state);
      return;
    } else if (
      acquire_image_res != VK_SUCCESS && acquire_image_res != VK_SUBOPTIMAL_KHR
    ) {
      logs::fatal("Could not acquire swap chain image.");
    }
  }

  // Deferred stage
  {
    VkCommandBuffer *command_buffer = &frame_resources->deferred_command_buffer;
    record_deferred_command_buffer(vk_state, command_buffer,
      common_state->extent, vk_state->idx_frame, idx_image);
    VkSemaphore const wait_semaphores[] = {
      frame_resources->image_available_semaphore
    };
    VkPipelineStageFlags const wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSemaphore const signal_semaphores[] = {
      vk_state->deferred_stage.render_finished_semaphore
    };
    VkSubmitInfo const submit_info = {
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = wait_semaphores,
      .pWaitDstStageMask    = wait_stages,
      .commandBufferCount   = 1,
      .pCommandBuffers      = command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = signal_semaphores,
    };
    check_vk_result(vkQueueSubmit(vk_state->graphics_queue, 1, &submit_info,
      nullptr));
  }

  // Main render stage
  {
    VkCommandBuffer *command_buffer = &frame_resources->main_command_buffer;
    record_main_command_buffer(vk_state, command_buffer,
      common_state->extent, vk_state->idx_frame, idx_image);
    VkSemaphore const wait_semaphores[] = {
      vk_state->deferred_stage.render_finished_semaphore
    };
    VkPipelineStageFlags const wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSemaphore const signal_semaphores[] = {
      vk_state->main_stage.render_finished_semaphore
    };
    VkSubmitInfo const submit_info = {
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = wait_semaphores,
      .pWaitDstStageMask    = wait_stages,
      .commandBufferCount   = 1,
      .pCommandBuffers      = command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = signal_semaphores,
    };
    vkResetFences(vk_state->device, 1, &frame_resources->frame_rendered_fence);
    check_vk_result(vkQueueSubmit(vk_state->graphics_queue, 1, &submit_info,
      frame_resources->frame_rendered_fence));
  }

  // Present image
  {
    VkSemaphore const signal_semaphores[] = {
      vk_state->main_stage.render_finished_semaphore
    };
    VkSwapchainKHR const swapchains[] = {vk_state->swapchain};
    VkPresentInfoKHR const present_info = {
      .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores    = signal_semaphores,
      .swapchainCount     = 1,
      .pSwapchains        = swapchains,
      .pImageIndices      = &idx_image,
    };
    VkResult const present_res = vkQueuePresentKHR(vk_state->present_queue,
      &present_info);

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
