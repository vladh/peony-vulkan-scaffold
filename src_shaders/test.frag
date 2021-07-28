#version 450

layout(binding = 1) uniform sampler2D tex_sampler;

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 tex_coords;

layout(location = 0) out vec4 out_color;

void main() {
  out_color = texture(tex_sampler, tex_coords);
}
