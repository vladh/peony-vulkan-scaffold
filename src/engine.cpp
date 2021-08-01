#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "engine.hpp"


void engine::update(EngineState *engine_state, CommonState *common_state) {
  *engine_state = {
    .model = glm::rotate(
      glm::mat4(1.0f),
      0.0f * glm::radians(90.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)
    ),
    .view = glm::lookAt(
      glm::vec3(2.0f, 2.0f, 2.0f),
      glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)
    ),
    .projection = glm::perspective(
      glm::radians(45.0f),
      (f32)common_state->extent.width / (f32)common_state->extent.height,
      0.01f,
      20.0f
    ),
  };

  // In OpenGL (which GLM was designed for), the y coordinate of the clip coordinates
  // is inverted. This is not true in Vulkan, so we invert it back.
  engine_state->projection[1][1] *= -1;
}
