/*
  Peony Game Engine
  Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
  All rights reserved.
*/

#pragma once

#include <chrono>
namespace chrono = std::chrono;
#include <assimp/cimport.h>
#include "types.hpp"

namespace util {
  GLenum get_texture_format_from_n_components(int32 n_components);
  f64 random(f64 min, f64 max);
  v3 aiVector3D_to_glm(aiVector3D *vec);
  quat aiQuaternion_to_glm(aiQuaternion *rotation);
  m4 aimatrix4x4_to_glm(aiMatrix4x4 *from);
  void APIENTRY debug_message_callback(
    GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char *message,
    const void *userParam
  );
  f32 round_to_nearest_multiple(f32 n, f32 multiple_of);
  f64 get_us_from_duration(chrono::duration<f64> duration);
  v3 get_orthogonal_vector(v3 *v);
  uint32 kb_to_b(uint32 value);
  uint32 mb_to_b(uint32 value);
  uint32 gb_to_b(uint32 value);
  uint32 tb_to_b(uint32 value);
  f32 b_to_kb(uint32 value);
  f32 b_to_mb(uint32 value);
  f32 b_to_gb(uint32 value);
  f32 b_to_tb(uint32 value);
}
