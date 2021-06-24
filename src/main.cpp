#include <stdio.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.hpp"
#include "intrinsics.hpp"


struct State {
  GLFWwindow *window;
};


void init_window(GLFWwindow **window) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  *window = glfwCreateWindow(800, 600, "Hi! :)", nullptr, nullptr);
}


void destroy_window(GLFWwindow *window) {
  glfwDestroyWindow(window);
  glfwTerminate();
}


void run_main_loop(State *state) {
  while (!glfwWindowShouldClose(state->window)) {
    glfwPollEvents();
  }
}


int main() {
  State *state = (State*)malloc(sizeof(State));
  defer { free(state); };

  init_window(&state->window);
  defer { destroy_window(state->window); };

  run_main_loop(state);

  return 0;
}
