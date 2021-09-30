#version 450

layout (set = 1, binding = 0) uniform sampler2D g_position;
layout (set = 1, binding = 1) uniform sampler2D g_normal;
layout (set = 1, binding = 2) uniform sampler2D g_albedo;
layout (set = 1, binding = 3) uniform sampler2D g_pbr;

layout (location = 0) in BLOCK {
  vec2 tex_coords;
} fs_in;

layout (location = 0) out vec4 color;

void main() {
  // if (fs_in.tex_coords.x < 0.4) {
  //   color = texture(g_position, fs_in.tex_coords);
  // } else if (fs_in.tex_coords.x < 0.5) {
  //   color = texture(g_normal, fs_in.tex_coords);
  // } else if (fs_in.tex_coords.x < 0.6) {
  //   color = texture(g_albedo, fs_in.tex_coords);
  // } else {
  //   color = texture(g_pbr, fs_in.tex_coords);
  // }
  color = texture(g_position, fs_in.tex_coords);
}
