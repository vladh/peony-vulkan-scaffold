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
  vec3 world_position;
  vec3 normal;
  vec2 tex_coords;
} vs_out;

void main() {
  vs_out.tex_coords = tex_coords;
  vs_out.world_position = vec3(ubo.model_matrix * vec4(position, 1.0));
  vs_out.normal = normalize(mat3(ubo.model_normal_matrix) * normal);
  gl_Position = ubo.projection * ubo.view * vec4(vs_out.world_position, 1.0);
}
