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

#include "vkutils.hpp"
#include "vulkan_core.cpp"
#include "vulkan_rendering.cpp"
#include "vulkan_stage_common.cpp"
#include "vulkan_stage_geometry.cpp"
#include "vulkan_stage_lighting.cpp"
#include "vulkan_stage_forward.cpp"
#include "vulkan_resources.cpp"


static std::thread loading_thread;


namespace vulkan {
  static constexpr VkDescriptorSetLayoutBinding DESCRIPTOR_BINDINGS[] = {
    vkutils::descriptor_set_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
  };
  static constexpr u32 N_DESCRIPTORS = LEN(DESCRIPTOR_BINDINGS);


  void init(VkState *vk_state, CommonState *common_state) {
    core::init(vk_state, common_state->window, &common_state->extent);

    // We only one command pool which we use for everything graphics-related
    vkutils::create_command_pool(vk_state->device, &vk_state->command_pool,
      (u32)vk_state->queue_family_indices.graphics);
    // We create another command pool for asset loading
    vkutils::create_command_pool(vk_state->device, &vk_state->asset_command_pool,
      (u32)vk_state->queue_family_indices.graphics);

    resources::init_static_textures(vk_state);
    resources::init_textures(vk_state);
    /* loading_thread = std::thread(resources::init_textures, vk_state); */
    resources::init_entities(vk_state);
    resources::init_uniform_buffers(vk_state);

    // Descriptors
    {
      // Create descriptor set layout
      auto const layout_info = vkutils::descriptor_set_layout_create_info(vulkan::N_DESCRIPTORS,
        vulkan::DESCRIPTOR_BINDINGS);
      vkutils::check(vkCreateDescriptorSetLayout(vk_state->device, &layout_info, nullptr,
        &vk_state->global_descriptor_set_layout));

      range (0, N_PARALLEL_FRAMES) {
        // Allocate descriptor sets
        auto const alloc_info = vkutils::descriptor_set_allocate_info(vk_state->descriptor_pool,
          &vk_state->global_descriptor_set_layout);
        vkutils::check(vkAllocateDescriptorSets(vk_state->device, &alloc_info, &vk_state->global_descriptor_sets[idx]));

        // Update descriptor sets
        VkDescriptorBufferInfo const buffer_info = {
          .buffer = vk_state->frame_resources[idx].uniform_buffer,
          .offset = 0,
          .range  = sizeof(CoreSceneState),
        };
        VkWriteDescriptorSet descriptor_writes[] = {
          vkutils::write_descriptor_set_buffer(vk_state->global_descriptor_sets[idx], 0, &buffer_info),
        };
        vkUpdateDescriptorSets(vk_state->device, vulkan::N_DESCRIPTORS, descriptor_writes, 0, nullptr);
      }
    }

    // Init render stages
    geometry_stage::init(vk_state, common_state->extent);
    lighting_stage::init(vk_state, common_state->extent);
    forward_stage::init(vk_state, common_state->extent);

    // Create semaphores and fences
    range (0, N_PARALLEL_FRAMES) {
      FrameResources *frame_resources = &vk_state->frame_resources[idx];
      vkutils::create_semaphore(vk_state->device, &frame_resources->image_available_semaphore);
      vkutils::create_fence(vk_state->device, &frame_resources->frame_rendered_fence);
    }
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

    geometry_stage::destroy_swapchain(vk_state);
    lighting_stage::destroy_swapchain(vk_state);
    forward_stage::destroy_swapchain(vk_state);

    vkDestroyDescriptorSetLayout(vk_state->device, vk_state->global_descriptor_set_layout, nullptr);

    range (0, vk_state->n_swapchain_images) {
      vkDestroyImageView(vk_state->device, vk_state->swapchain_image_views[idx], nullptr);
    }
  }


  void destroy(VkState *vk_state) {
    destroy_swapchain(vk_state);

    resources::destroy_static_textures(vk_state);
    resources::destroy_textures(vk_state);
    resources::destroy_entities(vk_state);

    range (0, N_PARALLEL_FRAMES) {
      FrameResources *frame_resources = &vk_state->frame_resources[idx];
      vkDestroySemaphore(vk_state->device, frame_resources->image_available_semaphore, nullptr);
      vkDestroyFence(vk_state->device, frame_resources->frame_rendered_fence, nullptr);
    }

    geometry_stage::destroy_nonswapchain(vk_state);
    lighting_stage::destroy_nonswapchain(vk_state);
    forward_stage::destroy_nonswapchain(vk_state);

    vkDestroyCommandPool(vk_state->device, vk_state->command_pool, nullptr);
    vkDestroyCommandPool(vk_state->device, vk_state->asset_command_pool, nullptr);

    core::destroy(vk_state);

    /* loading_thread.join(); */
  }


  void recreate_swapchain(VkState *vk_state, CommonState *common_state) {
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

    core::init_support_details(&vk_state->swapchain_support_details, vk_state->physical_device, vk_state->surface);
    core::init(vk_state, common_state->window, &common_state->extent);
    resources::init_uniform_buffers(vk_state);

    geometry_stage::init_swapchain(vk_state, common_state->extent);
    lighting_stage::init_swapchain(vk_state, common_state->extent);
    forward_stage::init_swapchain(vk_state, common_state->extent);
  }


  void render(VkState *vk_state, CommonState *common_state) {
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
    geometry_stage::render(vk_state, common_state->extent, idx_image);
    lighting_stage::render(vk_state, common_state->extent, idx_image);
    forward_stage::render(vk_state, common_state->extent, idx_image);

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


  void wait(VkState *vk_state) {
    vkQueueWaitIdle(vk_state->present_queue);
    vkDeviceWaitIdle(vk_state->device);
  }
}
