/*
  Peony Game Engine
  Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
  All rights reserved.
*/

#include "logs.hpp"
#include "memory.hpp"
#include "stb.hpp"
#include "files.hpp"


unsigned char* files::load_image(
  const char *path, int32 *width, int32 *height, int32 *n_channels,
  int32 desired_channels, bool should_flip
) {
  stbi_set_flip_vertically_on_load(should_flip);
  unsigned char *image_data = stbi_load(path, width, height, n_channels,
    desired_channels);
  if (!image_data) {
    logs::fatal("Could not open file %s: (strerror: %s) (stbi_failure_reason: %s)",
      path, strerror(errno), stbi_failure_reason());
  }
  return image_data;
}


void files::free_image(unsigned char *image_data) {
  stbi_image_free(image_data);
}


u32 files::get_file_size(char const * const path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    logs::error("Could not open file %s.", path);
    return 0;
  }
  fseek(f, 0, SEEK_END);
  u32 size = ftell(f);
  fclose(f);
  return size;
}


char* files::load_file_to_pool_str(
  MemoryPool *memory_pool, char const *path, size_t *file_size
) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    logs::error("Could not open file %s.", path);
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  *file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buffer = (char*)memory::push(memory_pool, (*file_size) + 1, path);
  size_t result = fread(buffer, *file_size, 1, f);
  fclose(f);
  if (result != 1) {
    logs::error("Could not read from file %s.", path);
    return nullptr;
  }

  buffer[*file_size] = 0;
  return buffer;
}


u8* files::load_file_to_pool_u8(
  MemoryPool *memory_pool, char const *path, size_t *file_size
) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    logs::error("Could not open file %s.", path);
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  *file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  u8 *buffer = (u8*)memory::push(memory_pool, *file_size, path);
  size_t result = fread(buffer, *file_size, 1, f);
  fclose(f);
  if (result != 1) {
    logs::error("Could not read from file %s.", path);
    return nullptr;
  }

  return buffer;
}


char* files::load_file_to_str(
  char *buffer, char const *path, size_t *file_size
) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    logs::error("Could not open file %s.", path);
    return nullptr;
  }
  fseek(f, 0, SEEK_END);
  *file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  size_t result = fread(buffer, *file_size, 1, f);
  fclose(f);
  if (result != 1) {
    logs::error("Could not read from file %s.", path);
    return nullptr;
  }

  buffer[*file_size] = 0;
  return buffer;
}
