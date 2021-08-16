#include <stdio.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.hpp"
#include "intrinsics.hpp"
#include "common.hpp"
#include "vulkan.hpp"
#include "engine.hpp"


struct State {
  CommonState common_state;
  VkState vk_state;
};


static void framebuffer_size_callback(
  GLFWwindow* window, int width, int height
) {
  State *state = (State*)glfwGetWindowUserPointer(window);
  state->vk_state.should_recreate_swapchain = true;
}


void key_callback(
  GLFWwindow* window, int key, int scancode, int action, int mods
) {
  State *state = (State*)glfwGetWindowUserPointer(window);

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    state->common_state.should_quit = true;
  }
}



static void init_window(GLFWwindow **window, State *state) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  *window = glfwCreateWindow(1600, 1000, "Hi! :)", nullptr, nullptr);
  glfwSetWindowUserPointer(*window, state);
  glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);
  glfwSetKeyCallback(*window, key_callback);
}


static void destroy_window(GLFWwindow *window) {
  glfwDestroyWindow(window);
  glfwTerminate();
}


static void run_main_loop(State *state) {
  while (
    !glfwWindowShouldClose(state->common_state.window) &&
    !state->common_state.should_quit
  ) {
    glfwPollEvents();
    engine::update(&state->common_state);
    vulkan::render(&state->vk_state, &state->common_state);
    vulkan::wait(&state->vk_state);
  }
}


int main() {
  State *state = (State*)calloc(1, sizeof(State));
  defer { free(state); };

  init_window(&state->common_state.window, state);
  defer { destroy_window(state->common_state.window); };

  vulkan::init(&state->vk_state, &state->common_state);
  defer { vulkan::destroy(&state->vk_state); };

  run_main_loop(state);

  return 0;
}
