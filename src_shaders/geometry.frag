#version 450

layout (set = 1, binding = 0) uniform sampler2D tex_sampler;

layout (location = 0) in BLOCK {
  vec3 world_position;
  vec3 normal;
  vec2 tex_coords;
} fs_in;

layout (location = 0) out vec4 g_position;
layout (location = 1) out vec4 g_normal;
layout (location = 2) out vec4 g_albedo;
layout (location = 3) out vec4 g_pbr;

void main() {
  g_albedo = texture(tex_sampler, fs_in.tex_coords);
  g_position = vec4(fs_in.world_position, 1.0);
  g_normal = vec4(fs_in.normal, 1.0);
  g_pbr = vec4(0.0, 0.0, 1.0, 1.0f);
}
