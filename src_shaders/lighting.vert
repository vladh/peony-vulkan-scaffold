#version 450

layout (set = 0, binding = 0) uniform CoreSceneState {
  mat4 model_matrix;
  mat4 model_normal_matrix; // actually mat3, we are using mat4 for padding
  mat4 view;
  mat4 projection;
} ubo;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 tex_coords;

layout (location = 0) out BLOCK {
  vec2 tex_coords;
} vs_out;

void main() {
  vs_out.tex_coords = tex_coords;
  gl_Position = vec4(position, 1.0);
}
