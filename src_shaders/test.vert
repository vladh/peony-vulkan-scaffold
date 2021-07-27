#version 450

layout(binding = 0) uniform ShaderCommon {
  mat4 model;
  mat4 view;
  mat4 projection;
} ubo;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 frag_color;

void main() {
  gl_Position = ubo.projection * ubo.view * ubo.model * vec4(position, 1.0);
  frag_color = color;
}
