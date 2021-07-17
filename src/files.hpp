/*
  Peony Game Engine
  Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
  All rights reserved.
*/

#pragma once

#include "types.hpp"
#include "memory.hpp"

namespace files {
  unsigned char* load_image(
    const char *path, int32 *width, int32 *height, int32 *n_channels, bool should_flip
  );
  unsigned char* load_image(
    const char *path, int32 *width, int32 *height, int32 *n_channels
  );
  void free_image(unsigned char *image_data);
  uint32 get_file_size(const char *path);
  const char* load_file(
    MemoryPool *memory_pool, const char *path, uint32 *file_size
  );
  const char* load_file(
    char *string, const char *path, uint32 *file_size
  );
}
