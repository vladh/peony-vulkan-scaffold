#version 450

layout (binding = 1) uniform sampler2D g_position;
layout (binding = 2) uniform sampler2D g_normal;
layout (binding = 3) uniform sampler2D g_albedo;
layout (binding = 4) uniform sampler2D g_pbr;

layout (location = 0) in BLOCK {
  vec3 world_position;
  vec3 normal;
  vec2 tex_coords;
} fs_in;

layout (location = 0) out vec4 color;

void main() {
  color = texture(g_position, fs_in.tex_coords);
}
