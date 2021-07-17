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

  u32 get_file_size(char const * const path);
  char* load_file_to_pool_str(
    MemoryPool *memory_pool, char const *path, size_t *file_size
  );
  u8* load_file_to_pool_u8(
    MemoryPool *memory_pool, char const *path, size_t *file_size
  );
  char* load_file_to_str(
    char *buffer, char const *path, size_t *file_size
  );
}
