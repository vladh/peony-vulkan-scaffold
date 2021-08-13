#pragma once

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct CoreSceneState {
  m4 model_matrix;
  m4 model_normal_matrix; // actually m3, we are using m4 for padding
  m4 view;
  m4 projection;
};

struct CommonState {
  GLFWwindow *window;
  VkExtent2D extent;
  CoreSceneState core_scene_state;
};
