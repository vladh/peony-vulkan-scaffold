/*
  Functions that load all the resources we need into Vulkan objects.
*/

#include <thread>
#include <chrono>
#include <filesystem>
#include <iostream>
#include "stb.hpp"
#include "vulkan.hpp"
#include "vkutils.hpp"


namespace vulkan::resources {
  static void init_static_textures(VkState *vk_state) {
    // Load dummy
    {
      constexpr u32 width = 1;
      constexpr u32 height = 1;
      constexpr u32 n_channels = 4;
      unsigned char image[width * height * n_channels] = {0};

      vkutils::create_image_resources_with_sampler(
        vk_state->device,
        &vk_state->dummy_image,
        vk_state->physical_device,
        width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        vk_state->physical_device_properties);

      vkutils::upload_image(
        vk_state->device,
        &vk_state->dummy_image,
        vk_state->physical_device,
        image,
        width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        vk_state->graphics_queue,
        vk_state->command_pool);
    }
  }


  static void destroy_static_textures(VkState *vk_state) {
    vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->dummy_image);
  }


  static void init_textures(VkState *vk_state) {
    // Load alpaca
    {
      int width, height, n_channels;
      unsigned char *image = files::load_image("../peony/resources/textures/alpaca.jpg", &width, &height, &n_channels,
        STBI_rgb_alpha, false);
      defer { files::free_image(image); };

      /* std::this_thread::sleep_for(std::chrono::milliseconds(2500)); */

      vkutils::create_image_resources_with_sampler(
        vk_state->device,
        &vk_state->alpaca,
        vk_state->physical_device,
        width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        vk_state->physical_device_properties);

      vkutils::upload_image(
        vk_state->device,
        &vk_state->alpaca,
        vk_state->physical_device,
        image,
        width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        vk_state->asset_queue,
        vk_state->asset_command_pool);
    }
  }


  static void destroy_textures(VkState *vk_state) {
    vkutils::destroy_image_resources_with_sampler(vk_state->device, &vk_state->alpaca);
  }


  static void init_uniform_buffers(VkState *vk_state) {
    range (0, N_PARALLEL_FRAMES) {
      FrameResources *frame_resources = &vk_state->frame_resources[idx];
      vkutils::create_buffer(vk_state->device, vk_state->physical_device,
        sizeof(CoreSceneState),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &frame_resources->uniform_buffer,
        &frame_resources->uniform_buffer_memory);
    }
  }


  static void init_entities(VkState *vk_state) {
    // TODO: #slow Allocate memory only once, and split that up ourselves into the
    // two buffers using the memory offsets in e.g. `vkCmdBindVertexBuffers()`.
    // vulkan-tutorial.com/Vertex_buffers/Index_buffer.html

    // Screenquad
    {
      DrawableComponent *screenquad = &vk_state->drawable_components[vk_state->n_entities++];
      *screenquad = {
        .target_render_stages = RenderStageName::lighting,
      };
      vkutils::create_buffer_resources(vk_state->device,
        &screenquad->vertex,
        vk_state->physical_device,
        SCREENQUAD_VERTICES,
        LEN(SCREENQUAD_VERTICES),
        sizeof(SCREENQUAD_VERTICES),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vk_state->command_pool,
        vk_state->graphics_queue);
      vkutils::create_buffer_resources(vk_state->device,
        &screenquad->index,
        vk_state->physical_device,
        SCREENQUAD_INDICES,
        LEN(SCREENQUAD_INDICES),
        sizeof(SCREENQUAD_INDICES),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        vk_state->command_pool,
        vk_state->graphics_queue);
    }

    // Top sign
    {
      DrawableComponent *sign = &vk_state->drawable_components[vk_state->n_entities++];
      *sign = {
        .target_render_stages = RenderStageName::geometry,
        .position = v3(0.0f, 0.0f, 0.0f),
      };
      vkutils::create_buffer_resources(vk_state->device,
        &sign->vertex,
        vk_state->physical_device,
        SIGN_VERTICES,
        LEN(SIGN_VERTICES),
        sizeof(SIGN_VERTICES),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vk_state->command_pool,
        vk_state->graphics_queue);
      vkutils::create_buffer_resources(vk_state->device,
        &sign->index,
        vk_state->physical_device,
        SIGN_INDICES,
        LEN(SIGN_INDICES),
        sizeof(SIGN_INDICES),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        vk_state->command_pool,
        vk_state->graphics_queue);
    }

    // Bottom sign
    {
      DrawableComponent *sign = &vk_state->drawable_components[vk_state->n_entities++];
      *sign = {
        .target_render_stages = RenderStageName::forward_depth,
        .position = v3(0.0f, -1.0f, 0.0f),
      };
      vkutils::create_buffer_resources(vk_state->device,
        &sign->vertex,
        vk_state->physical_device,
        SIGN_VERTICES,
        LEN(SIGN_VERTICES),
        sizeof(SIGN_VERTICES),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vk_state->command_pool,
        vk_state->graphics_queue);
      vkutils::create_buffer_resources(vk_state->device,
        &sign->index,
        vk_state->physical_device,
        SIGN_INDICES,
        LEN(SIGN_INDICES),
        sizeof(SIGN_INDICES),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        vk_state->command_pool,
        vk_state->graphics_queue);
    }
  }


  static void destroy_entities(VkState *vk_state) {
    range (0, vk_state->n_entities) {
      vkutils::destroy_buffer_resources(vk_state->device, &vk_state->drawable_components[idx].vertex);
      vkutils::destroy_buffer_resources(vk_state->device, &vk_state->drawable_components[idx].index);
    }
  }
}
