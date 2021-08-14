#version 450

layout (binding = 1) uniform sampler2D tex_sampler;

layout (location = 0) in BLOCK {
  vec3 world_position;
  vec3 normal;
  vec2 tex_coords;
} fs_in;

layout (location = 0) out vec4 color;

void main() {
  color = texture(tex_sampler, fs_in.tex_coords);
}
