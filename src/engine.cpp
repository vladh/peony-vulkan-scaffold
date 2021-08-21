#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "engine.hpp"


void engine::update(CommonState *common_state) {
  f64 t = glfwGetTime();
  m4 model_matrix = rotate(m4(1.0f), (f32)t, v3(0.0f, 1.0f, 0.0f));
  m4 model_normal_matrix = m4(m3(transpose(inverse(model_matrix))));
  common_state->core_scene_state = {
    .model_matrix = model_matrix,
    .model_normal_matrix = model_normal_matrix,
    .view = glm::lookAt(
      v3(-1.0f, 1.0f, 1.0f),
      v3(0.0f, 0.0f, 0.0f),
      v3(0.0f, 1.0f, 0.0f)
    ),
    .projection = glm::perspective(
      radians(90.0f),
      (f32)common_state->extent.width / (f32)common_state->extent.height,
      0.01f,
      20.0f
    ),
  };

  // In OpenGL (which GLM was designed for), the y coordinate of the clip
  // coordinates is inverted. This is not true in Vulkan, so we invert it back.
  common_state->core_scene_state.projection[1][1] *= -1;
}
