#include "vulkan.hpp"
#include "vulkan_rendering.hpp"


void vulkan::rendering::render_drawable_component(DrawableComponent *drawable, VkCommandBuffer *command_buffer) {
  VkBuffer const vertex_buffers[] = {drawable->vertex.buffer};
  VkDeviceSize const offsets[] = {0};

  vkCmdBindVertexBuffers(*command_buffer, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(*command_buffer, drawable->index.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(*command_buffer, drawable->index.n_items, 1, 0, 0, 0);
}
