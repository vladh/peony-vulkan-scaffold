#pragma once

struct Extent {
  u32 width;
  u32 height;
};

struct CommonState {
  GLFWwindow *window;
  Extent extent;
};
