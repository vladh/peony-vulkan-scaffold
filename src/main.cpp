#include <stdio.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan.hpp"
#include "types.hpp"
#include "intrinsics.hpp"


struct State {
  GLFWwindow *window;
  VkState vk_state;
};


static void init_window(GLFWwindow **window) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  *window = glfwCreateWindow(800, 600, "Hi! :)", nullptr, nullptr);
}


static void destroy_window(GLFWwindow *window) {
  glfwDestroyWindow(window);
  glfwTerminate();
}


static void run_main_loop(State *state) {
  while (!glfwWindowShouldClose(state->window)) {
    glfwPollEvents();
    vulkan::render(&state->vk_state);
  }
  vulkan::wait(&state->vk_state);
}


int main() {
  State *state = (State*)calloc(1, sizeof(State));
  defer { free(state); };

  init_window(&state->window);
  defer { destroy_window(state->window); };

  vulkan::init(&state->vk_state, state->window);
  defer { vulkan::destroy(&state->vk_state); };

  run_main_loop(state);

  return 0;
}
