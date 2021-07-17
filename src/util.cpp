/*
  Peony Game Engine
  Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
  All rights reserved.
*/

#include "logs.hpp"
#include "util.hpp"


real64 util::random(real64 min, real64 max) {
  uint32 r = rand();
  real64 r_normalized = (real64)r / (real64)RAND_MAX;
  return min + ((r_normalized) * (max - min));
}


v3 util::aiVector3D_to_glm(aiVector3D *vec) {
  return v3(vec->x, vec->y, vec->z);
}


quat util::aiQuaternion_to_glm(aiQuaternion *rotation) {
  return quat(rotation->w, rotation->x, rotation->y, rotation->z);
}


real32 util::round_to_nearest_multiple(real32 n, real32 multiple_of) {
  return (floor((n) / multiple_of) * multiple_of) + multiple_of;
}


real64 util::get_us_from_duration(chrono::duration<real64> duration) {
  return chrono::duration_cast<chrono::duration<real64>>(duration).count();
}


v3 util::get_orthogonal_vector(v3 *v) {
  if (v->z < v->x) {
    return v3(v->y, -v->x, 0.0f);
  } else {
    return v3(0.0f, -v->z, v->y);
  }
}


uint32 util::kb_to_b(uint32 value) { return value * 1024; }
uint32 util::mb_to_b(uint32 value) { return kb_to_b(value) * 1024; }
uint32 util::gb_to_b(uint32 value) { return mb_to_b(value) * 1024; }
uint32 util::tb_to_b(uint32 value) { return gb_to_b(value) * 1024; }
real32 util::b_to_kb(uint32 value) { return value / 1024.0f; }
real32 util::b_to_mb(uint32 value) { return b_to_kb(value) / 1024.0f; }
real32 util::b_to_gb(uint32 value) { return b_to_mb(value) / 1024.0f; }
real32 util::b_to_tb(uint32 value) { return b_to_gb(value) / 1024.0f; }
